// Minimal test framework.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures() {
    static int f = 0;
    return f;
}

inline void registerTest(const std::string& name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
}

inline int runAll() {
    for (const auto& test : registry()) {
        int before = failures();
        try {
            test.fn();
        } catch (const std::exception& e) {
            ++failures();
            std::printf("  uncaught exception: %s\n", e.what());
        } catch (...) {
            ++failures();
            std::printf("  unknown exception\n");
        }
        std::printf("[%s] %s\n", failures() == before ? "PASS" : "FAIL", test.name.c_str());
    }
    std::printf("\n%d test(s), %d failure(s)\n", static_cast<int>(registry().size()), failures());
    return failures() == 0 ? 0 : 1;
}

} // namespace testing

#define TEST(name)                                                        \
    static void test_##name();                                            \
    static const bool registered_##name = [] {                            \
        testing::registerTest(#name, test_##name);                        \
        return true;                                                      \
    }();                                                                  \
    static void test_##name()

#define EXPECT_TRUE(cond)                                                 \
    do {                                                                  \
        if (!(cond)) {                                                    \
            ++testing::failures();                                        \
            std::printf("  EXPECT_TRUE failed at %s:%d: %s\n", __FILE__,  \
                        __LINE__, #cond);                                 \
        }                                                                 \
    } while (0)

#define EXPECT_EQ(a, b)                                                   \
    do {                                                                  \
        auto va = (a);                                                    \
        auto vb = (b);                                                    \
        if (!(va == vb)) {                                                \
            ++testing::failures();                                        \
            std::printf("  EXPECT_EQ failed at %s:%d: %s\n", __FILE__,    \
                        __LINE__, #a " != " #b);                          \
        }                                                                 \
    } while (0)

#define EXPECT_BYTES(a, an, b, bn)                                        \
    do {                                                                  \
        if ((an) != (bn) || memcmp((a), (b), (an)) != 0) {                \
            ++testing::failures();                                        \
            std::printf("  EXPECT_BYTES failed at %s:%d\n", __FILE__,     \
                        __LINE__);                                        \
        }                                                                 \
    } while (0)
