#pragma once
#include <cstdio>
#include <cmath>
#include <functional>
#include <vector>

struct TestCase { const char* name; std::function<void()> fn; };
inline std::vector<TestCase>& allTests() { static std::vector<TestCase> t; return t; }
inline int& failCount() { static int f = 0; return f; }
struct TestReg { TestReg(const char* n, std::function<void()> f) { allTests().push_back({n, std::move(f)}); } };

#define TEST(name) \
    static void test_##name(); \
    static TestReg reg_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failCount()++; } } while (0)

#define CHECK_NEAR(a, b, tol) do { double _a=(a), _b=(b); if (std::fabs(_a-_b) > (tol)) { \
    std::printf("FAIL %s:%d: %s=%g vs %s=%g\n", __FILE__, __LINE__, #a, _a, #b, _b); failCount()++; } } while (0)
