#pragma once
// Rack-free, jansson-free HTTP/1.1 request parsing for the UAT bridge. Kept
// pure so the headless unit suite covers it (tests/unit/test_uathttp.cpp).
#include <cctype>
#include <cstdlib>
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

// Lowercased copy of the header block (request line included), so header
// lookups can be case-insensitive without touching the body.
inline std::string headerBlockLower(const std::string& raw) {
    size_t he = raw.find("\r\n\r\n");
    std::string head = raw.substr(0, he == std::string::npos ? raw.size() : he);
    for (char& c : head) c = (char)std::tolower((unsigned char)c);
    return head;
}

// Byte length of the first complete request in `raw` (headers + CRLFCRLF +
// Content-Length body), or 0 if more bytes are still needed. With keep-alive
// a socket carries several requests back to back, so the reader has to frame
// each one instead of treating "everything received so far" as one request.
inline size_t requestLength(const std::string& raw) {
    size_t he = raw.find("\r\n\r\n");
    if (he == std::string::npos) return 0;
    size_t cl = 0;
    size_t pos = raw.find("\r\n");  // skip the request line
    while (pos != std::string::npos && pos < he) {
        size_t next = raw.find("\r\n", pos + 2);
        std::string line = raw.substr(pos + 2, next - (pos + 2));
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            for (char& c : name) c = (char)std::tolower((unsigned char)c);
            if (name == "content-length")
                cl = (size_t)std::strtoul(line.c_str() + colon + 1, nullptr, 10);
        }
        pos = next;
    }
    size_t total = he + 4 + cl;
    return raw.size() >= total ? total : 0;
}

// Should this connection stay open after the response? HTTP/1.1 defaults to
// keep-alive unless the client says `Connection: close`; HTTP/1.0 (and any
// unparseable request line) keeps the old close-every-time behaviour.
inline bool wantsKeepAlive(const std::string& raw) {
    std::string head = headerBlockLower(raw);
    size_t lineEnd = head.find("\r\n");
    std::string reqLine = head.substr(0, lineEnd == std::string::npos ? head.size() : lineEnd);
    if (reqLine.find("http/1.1") == std::string::npos) return false;
    size_t c = head.find("\r\nconnection:");
    if (c == std::string::npos) return true;
    size_t valEnd = head.find("\r\n", c + 2);
    std::string val = head.substr(c + 13, valEnd - (c + 13));
    return val.find("close") == std::string::npos;
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

inline std::string makeResponse(int code, const std::string& jsonBody,
                                bool keepAlive = false) {
    const char* text = code == 200 ? "OK" : code == 404 ? "Not Found"
                     : code == 400 ? "Bad Request" : "Error";
    return "HTTP/1.1 " + std::to_string(code) + " " + text + "\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(jsonBody.size()) + "\r\n"
           "Connection: " + (keepAlive ? "keep-alive" : "close") + "\r\n\r\n" + jsonBody;
}

} // namespace uat
