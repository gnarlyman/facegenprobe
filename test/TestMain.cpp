#include "TestFramework.h"

int main() {
    int n = 0;
    for (auto& t : AllTests()) {
        std::printf("[ RUN  ] %s\n", t.name);
        t.fn();
        std::printf("[  OK  ] %s\n", t.name);
        ++n;
    }
    std::printf("\n%d tests passed.\n", n);
    return 0;
}

// Placeholder so the test runner compiles before any real tests exist.
TEST(smoke_runner_runs) {
    ASSERT_TRUE(1 + 1 == 2);
}
