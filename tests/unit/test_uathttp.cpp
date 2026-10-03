#include "harness.hpp"
#include "uatbridge/httpcore.hpp"

TEST(uathttp_parse_get_query) {
    uat::Request r;
    bool ok = uat::parseRequest(
        "GET /probe?moduleId=42&ms=500 HTTP/1.1\r\nHost: x\r\n\r\n", r);
    CHECK(ok);
    CHECK(r.method == "GET");
    CHECK(r.path == "/probe");
    CHECK(r.query.at("moduleId") == "42");
    CHECK(r.query.at("ms") == "500");
}

TEST(uathttp_parse_post_body) {
    uat::Request r;
    std::string raw =
        "POST /master/7/patch HTTP/1.1\r\nContent-Length: 16\r\n\r\n"
        "{\"path\":\"a.ini\"}";
    CHECK(uat::parseRequest(raw, r));
    CHECK(r.method == "POST");
    CHECK(r.body == "{\"path\":\"a.ini\"}");
    CHECK(uat::splitPath(r.path) ==
           (std::vector<std::string>{"master","7","patch"}));
}

TEST(uathttp_parse_rejects_garbage) {
    uat::Request r;
    CHECK(!uat::parseRequest("NONSENSE", r));
}

TEST(uathttp_make_response) {
    std::string s = uat::makeResponse(200, "{\"ok\":true}");
    CHECK(s.rfind("HTTP/1.1 200", 0) == 0);
    CHECK(s.find("Content-Length: 11") != std::string::npos);
    CHECK(s.find("\r\n\r\n{\"ok\":true}") != std::string::npos);
}

// --- keep-alive framing (issue #97) --------------------------------------

TEST(uathttp_request_length_frames_get) {
    std::string one = "GET /ping HTTP/1.1\r\nHost: x\r\n\r\n";
    CHECK(uat::requestLength(one) == one.size());
    CHECK(uat::requestLength("GET /ping HTTP/1.1\r\nHost: x\r\n") == 0);
}

TEST(uathttp_request_length_honours_content_length) {
    std::string head = "POST /x HTTP/1.1\r\ncontent-length: 5\r\n\r\n";
    CHECK(uat::requestLength(head) == 0);          // body still missing
    CHECK(uat::requestLength(head + "abc") == 0);  // body incomplete
    CHECK(uat::requestLength(head + "abcde") == head.size() + 5);
}

TEST(uathttp_request_length_splits_pipelined) {
    std::string a = "POST /a HTTP/1.1\r\nContent-Length: 2\r\n\r\n{}";
    std::string b = "GET /b HTTP/1.1\r\n\r\n";
    std::string both = a + b;
    size_t n = uat::requestLength(both);
    CHECK(n == a.size());
    uat::Request r;
    CHECK(uat::parseRequest(both.substr(0, n), r));
    CHECK(r.path == "/a");
    CHECK(r.body == "{}");          // the next request is NOT swallowed
    CHECK(uat::requestLength(both.substr(n)) == b.size());
}

TEST(uathttp_keepalive_defaults_by_version) {
    CHECK(uat::wantsKeepAlive("GET /ping HTTP/1.1\r\nHost: x\r\n\r\n"));
    CHECK(!uat::wantsKeepAlive("GET /ping HTTP/1.0\r\nHost: x\r\n\r\n"));
    CHECK(!uat::wantsKeepAlive("GET /ping HTTP/1.0\r\nConnection: keep-alive\r\n\r\n"));
    CHECK(!uat::wantsKeepAlive("GET /ping HTTP/1.1\r\nConnection: close\r\n\r\n"));
    CHECK(!uat::wantsKeepAlive("GET /ping HTTP/1.1\r\nconnection: Close\r\n\r\n"));
    CHECK(uat::wantsKeepAlive("GET /ping HTTP/1.1\r\nConnection: keep-alive\r\n\r\n"));
}

TEST(uathttp_response_connection_header) {
    CHECK(uat::makeResponse(200, "{}").find("Connection: close") != std::string::npos);
    CHECK(uat::makeResponse(200, "{}", true).find("Connection: keep-alive")
          != std::string::npos);
}
