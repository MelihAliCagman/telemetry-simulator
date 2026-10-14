#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "net/http_types.hpp"

namespace uavgcs {

// Boost.Beast tabanlı basit HTTP + WebSocket sunucusu.
//
// - Her bağlantı kendi iş parçacığında senkron çalışır (küçük yük için yeterli, kodu sade tutar).
// - `/ws` yolu WebSocket'e yükseltilir; istemciler yalnızca sunucudan veri alır (itme/yayın).
//   Komutlar REST üzerinden gelir.
class Server {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    static constexpr std::size_t kMaxConnections = 256;
    static constexpr std::size_t kMaxBodyBytes = 1 << 20;  // 1 MiB

    Server(std::string address, unsigned short port, Handler handler, std::string cors_origin);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Soketi bağlar ve kabul döngüsünü başlatır. Bağlanamazsa std::exception fırlatır.
    void start();
    void stop();

    // Tüm bağlı WebSocket istemcilerine metin mesajı gönderir; hatalı istemcileri düşürür.
    void broadcast(const std::string& text);
    std::size_t websocket_clients() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace uavgcs
