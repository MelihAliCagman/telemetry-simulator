#pragma once
#include <map>
#include <string>

namespace uavgcs {

struct HttpRequest {
    std::string method;                         // GET, POST, ...
    std::string path;                           // sorgu dizesi olmadan, örn. /api/uavs/1
    std::map<std::string, std::string> query;   // çözülmüş sorgu parametreleri
    std::string body;
};

struct HttpResponse {
    int status{200};
    std::string body;
    std::string content_type{"application/json; charset=utf-8"};
};

}  // namespace uavgcs
