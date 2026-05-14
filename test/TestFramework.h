#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& AllTests() {
    static std::vector<TestCase> v;
    return v;
}

struct TestRegistrar {
    TestRegistrar(const char* name, void (*fn)()) {
        AllTests().push_back({ name, fn });
    }
};

#define TEST(name)                                              \
    static void name();                                         \
    static TestRegistrar reg_##name(#name, name);               \
    static void name()

#define ASSERT_TRUE(x) do {                                     \
    if (!(x)) {                                                 \
        std::fprintf(stderr, "  FAIL: %s:%d: ASSERT_TRUE(%s)\n",\
            __FILE__, __LINE__, #x);                            \
        std::exit(1);                                           \
    }                                                           \
} while (0)

#define ASSERT_EQ(a, b) do {                                    \
    auto _a = (a); auto _b = (b);                               \
    if (!(_a == _b)) {                                          \
        std::fprintf(stderr, "  FAIL: %s:%d: ASSERT_EQ(%s, %s)\n",\
            __FILE__, __LINE__, #a, #b);                        \
        std::exit(1);                                           \
    }                                                           \
} while (0)

#define ASSERT_STREQ(a, b) do {                                 \
    const char* _a = (a); const char* _b = (b);                 \
    if (!_a || !_b) {                                           \
        std::fprintf(stderr, "  FAIL: %s:%d: ASSERT_STREQ nullptr arg (a=%p b=%p)\n",\
            __FILE__, __LINE__, (const void*)_a, (const void*)_b);\
        std::exit(1);                                           \
    }                                                           \
    if (std::strcmp(_a, _b) != 0) {                             \
        std::fprintf(stderr, "  FAIL: %s:%d: ASSERT_STREQ\n     got: '%s'\n     exp: '%s'\n",\
            __FILE__, __LINE__, _a, _b);                        \
        std::exit(1);                                           \
    }                                                           \
} while (0)
