// libpqxx'in derlendiği C++ standardıyla uyumlu ABI'yi tespit etmek için küçük bir bağlama denemesi.
// Çalıştırılmaz; yalnızca derlenip bağlanabilmesi önemlidir.
#include <pqxx/pqxx>
#include <string>

int main(int argc, char**) {
    if (argc > 5) {
        pqxx::connection conn{"x"};
        pqxx::work tx{conn};
        auto s = pqxx::stream_to::table(tx, {"t"}, {"a", "b"});
        s.write_values(std::string("x"), 1.5);
        s.complete();
        tx.commit();
    }
    return 0;
}
