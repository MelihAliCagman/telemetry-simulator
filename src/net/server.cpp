#include "net/server.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <chrono>
#include <cstdlib>
#include <thread>

#include "util/time.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace uavgcs {

namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && hex_value(s[i + 1]) >= 0 && hex_value(s[i + 2]) >= 0) {
            out.push_back(static_cast<char>(hex_value(s[i + 1]) * 16 + hex_value(s[i + 2])));
            i += 2;
        } else if (s[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

HttpRequest to_request(const http::request<http::string_body>& req) {
    HttpRequest out;
    out.method = std::string(req.method_string());
    out.body = req.body();

    const std::string target(req.target());
    const auto q = target.find('?');
    out.path = url_decode(target.substr(0, q));
    if (q != std::string::npos) {
        std::string rest = target.substr(q + 1);
        std::size_t pos = 0;
        while (pos <= rest.size()) {
            auto amp = rest.find('&', pos);
            if (amp == std::string::npos) amp = rest.size();
            const std::string pair = rest.substr(pos, amp - pos);
            if (!pair.empty()) {
                const auto eq = pair.find('=');
                const std::string key = url_decode(pair.substr(0, eq));
                const std::string val = eq == std::string::npos ? "" : url_decode(pair.substr(eq + 1));
                out.query[key] = val;
            }
            pos = amp + 1;
        }
    }
    return out;
}

using WsStream = websocket::stream<beast::tcp_stream>;

}  // namespace

struct Server::Impl {
    Impl(std::string addr, unsigned short p, Handler h, std::string cors)
        : address(std::move(addr)), port(p), handler(std::move(h)), cors_origin(std::move(cors)), acceptor(ioc) {}

    std::string address;
    unsigned short port;
    Handler handler;
    std::string cors_origin;

    net::io_context ioc;
    tcp::acceptor acceptor;
    std::thread accept_thread;
    std::atomic<bool> running{false};
    std::atomic<int> active_connections{0};

    mutable std::mutex clients_mutex;
    std::vector<std::shared_ptr<WsStream>> clients;

    void accept_loop();
    void serve(tcp::socket socket);
    void upgrade(beast::tcp_stream stream, http::request<http::string_body> req);

    template <class Body>
    void add_cors(http::response<Body>& res) const {
        res.set(http::field::access_control_allow_origin, cors_origin);
        res.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
        res.set(http::field::access_control_allow_headers, "Content-Type");
        res.set(http::field::vary, "Origin");
    }
};

void Server::Impl::accept_loop() {
    while (running) {
        tcp::socket socket(ioc);
        beast::error_code ec;
        acceptor.accept(socket, ec);
        if (ec) {
            if (!running) break;
            util::log_warn("accept başarısız: " + ec.message());
            continue;
        }
        if (active_connections >= static_cast<int>(kMaxConnections)) {
            // Aşırı yük: bağlantıyı sessizce kapat.
            socket.shutdown(tcp::socket::shutdown_both, ec);
            continue;
        }
        ++active_connections;
        std::thread([this, s = std::move(socket)]() mutable {
            try {
                serve(std::move(s));
            } catch (const std::exception& e) {
                util::log_warn(std::string("bağlantı hatası: ") + e.what());
            }
            --active_connections;
        }).detach();
    }
}

void Server::Impl::serve(tcp::socket socket) {
    beast::tcp_stream stream(std::move(socket));
    beast::flat_buffer buffer;
    beast::error_code ec;

    while (running) {
        http::request_parser<http::string_body> parser;
        parser.body_limit(kMaxBodyBytes);
        stream.expires_after(std::chrono::seconds(10));
        http::read(stream, buffer, parser, ec);
        if (ec == http::error::end_of_stream || ec == beast::error::timeout) break;
        if (ec) {
            if (ec == http::error::body_limit) {
                http::response<http::string_body> res{http::status::payload_too_large, 11};
                res.body() = "{\"error\":\"Gövde çok büyük\"}";
                res.set(http::field::content_type, "application/json; charset=utf-8");
                res.prepare_payload();
                http::write(stream, res, ec);
            }
            break;
        }
        auto req = parser.release();

        if (websocket::is_upgrade(req)) {
            if (req.target() != "/ws") {
                http::response<http::string_body> res{http::status::not_found, req.version()};
                res.prepare_payload();
                http::write(stream, res, ec);
                break;
            }
            upgrade(std::move(stream), std::move(req));
            return;
        }

        http::response<http::string_body> res;
        if (req.method() == http::verb::options) {
            res = http::response<http::string_body>{http::status::no_content, req.version()};
        } else {
            const HttpResponse out = handler(to_request(req));
            res = http::response<http::string_body>{static_cast<http::status>(out.status), req.version()};
            res.set(http::field::content_type, out.content_type);
            res.body() = out.body;
        }
        add_cors(res);
        res.set(http::field::server, "uav-gcs-simulator");
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        http::write(stream, res, ec);
        if (ec || !res.keep_alive()) break;
    }
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);
}

void Server::Impl::upgrade(beast::tcp_stream stream, http::request<http::string_body> req) {
    stream.expires_never();
    auto ws = std::make_shared<WsStream>(std::move(stream));
    ws->text(true);
    beast::error_code ec;
    ws->accept(req, ec);
    if (ec) return;

    // İstemci yalnızca dinler: okuma yapmadığımız için kapanış çerçevesi, bir sonraki yazma hatasıyla fark edilir.
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.push_back(std::move(ws));
}

Server::Server(std::string address, unsigned short port, Handler handler, std::string cors_origin)
    : impl_(std::make_unique<Impl>(std::move(address), port, std::move(handler), std::move(cors_origin))) {}

Server::~Server() { stop(); }

void Server::start() {
    auto& i = *impl_;
    const tcp::endpoint ep(net::ip::make_address(i.address), i.port);
    i.acceptor.open(ep.protocol());
    i.acceptor.set_option(net::socket_base::reuse_address(true));
    i.acceptor.bind(ep);
    i.acceptor.listen(net::socket_base::max_listen_connections);
    i.running = true;
    i.accept_thread = std::thread([this] { impl_->accept_loop(); });
}

void Server::stop() {
    auto& i = *impl_;
    if (!i.running.exchange(false)) return;

    beast::error_code ec;
    i.acceptor.close(ec);
    if (i.accept_thread.joinable()) i.accept_thread.join();

    {
        std::lock_guard<std::mutex> lock(i.clients_mutex);
        for (auto& c : i.clients) {
            beast::get_lowest_layer(*c).socket().shutdown(tcp::socket::shutdown_both, ec);
            beast::get_lowest_layer(*c).socket().close(ec);
        }
        i.clients.clear();
    }

    // Açık HTTP bağlantılarının (en fazla 10 sn boşta bekleme) bitmesini bekle.
    for (int n = 0; n < 120 && i.active_connections > 0; ++n) std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void Server::broadcast(const std::string& text) {
    std::lock_guard<std::mutex> lock(impl_->clients_mutex);
    auto& clients = impl_->clients;
    for (auto it = clients.begin(); it != clients.end();) {
        beast::error_code ec;
        beast::get_lowest_layer(**it).expires_after(std::chrono::seconds(2));
        (*it)->write(net::buffer(text), ec);
        if (ec) {
            beast::get_lowest_layer(**it).socket().close(ec);
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

std::size_t Server::websocket_clients() const {
    std::lock_guard<std::mutex> lock(impl_->clients_mutex);
    return impl_->clients.size();
}

}  // namespace uavgcs
