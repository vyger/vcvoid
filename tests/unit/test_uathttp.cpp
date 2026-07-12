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
