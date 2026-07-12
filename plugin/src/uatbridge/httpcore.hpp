#pragma once
// Rack-free, jansson-free HTTP/1.1 request parsing for the UAT bridge. Kept
// pure so the headless unit suite covers it (tests/unit/test_uathttp.cpp).
#include <map>
#include <string>
#include <vector>

namespace uat {

struct Request {
    std::string method, path, body;
    std::map<std::string, std::string> query;
};

inline std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : path) {
        if (c == '/') { if (!cur.empty()) parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

inline bool parseRequest(const std::string& raw, Request& out) {
    size_t lineEnd = raw.find("\r\n");
    if (lineEnd == std::string::npos) return false;
    std::string reqLine = raw.substr(0, lineEnd);
    size_t sp1 = reqLine.find(' '), sp2 = reqLine.rfind(' ');
    if (sp1 == std::string::npos || sp2 <= sp1) return false;
    out.method = reqLine.substr(0, sp1);
    std::string target = reqLine.substr(sp1 + 1, sp2 - sp1 - 1);
    if (target.empty() || target[0] != '/') return false;
    size_t q = target.find('?');
    out.path = target.substr(0, q);
    if (q != std::string::npos) {
        std::string qs = target.substr(q + 1);
        size_t pos = 0;
        while (pos < qs.size()) {
            size_t amp = qs.find('&', pos);
            std::string kv = qs.substr(pos, amp - pos);
            size_t eq = kv.find('=');
            if (eq != std::string::npos)
                out.query[kv.substr(0, eq)] = kv.substr(eq + 1);
            if (amp == std::string::npos) break;
            pos = amp + 1;
        }
    }
    size_t hdrEnd = raw.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) return false;
    out.body = raw.substr(hdrEnd + 4);
    return true;
}

inline std::string makeResponse(int code, const std::string& jsonBody) {
    const char* text = code == 200 ? "OK" : code == 404 ? "Not Found"
                     : code == 400 ? "Bad Request" : "Error";
    return "HTTP/1.1 " + std::to_string(code) + " " + text + "\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(jsonBody.size()) + "\r\n"
           "Connection: close\r\n\r\n" + jsonBody;
}

} // namespace uat
