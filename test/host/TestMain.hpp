// Minimal host-side test harness -- no external dependencies so `g++ *.cpp`
// is enough to run the suite anywhere.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
  const char* name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> cases;
  return cases;
}

inline int& failureCount() {
  static int failures = 0;
  return failures;
}

inline const char*& currentTest() {
  static const char* name = "";
  return name;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline void reportFailure(const char* file, int line, const std::string& what) {
  std::printf("  FAIL %s\n    at %s:%d\n    %s\n", currentTest(), file, line, what.c_str());
  ++failureCount();
}

inline bool nearlyEqual(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

inline int runAll() {
  std::printf("Running %zu test(s)\n", registry().size());
  for (const auto& tc : registry()) {
    currentTest() = tc.name;
    const int before = failureCount();
    tc.fn();
    if (failureCount() == before) std::printf("  ok   %s\n", tc.name);
  }
  if (failureCount() == 0) {
    std::printf("\nAll tests passed.\n");
    return 0;
  }
  std::printf("\n%d assertion(s) failed.\n", failureCount());
  return 1;
}

} // namespace testing

#define TEST(name)                                                    \
  static void name();                                                 \
  static testing::Registrar registrar_##name(#name, &name);           \
  static void name()

#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) testing::reportFailure(__FILE__, __LINE__, "expected: " #cond); \
  } while (0)

#define CHECK_EQ(actual, expected)                                    \
  do {                                                                \
    auto a_ = (actual);                                               \
    auto e_ = (expected);                                             \
    if (!(a_ == e_)) {                                                \
      testing::reportFailure(__FILE__, __LINE__,                      \
          std::string(#actual " == " #expected " -> got ") +          \
          std::to_string(a_) + ", want " + std::to_string(e_));       \
    }                                                                 \
  } while (0)

#define CHECK_NEAR(actual, expected, tol)                             \
  do {                                                                \
    double a_ = static_cast<double>(actual);                          \
    double e_ = static_cast<double>(expected);                        \
    if (!testing::nearlyEqual(a_, e_, tol)) {                         \
      testing::reportFailure(__FILE__, __LINE__,                      \
          std::string(#actual " ~= " #expected " -> got ") +          \
          std::to_string(a_) + ", want " + std::to_string(e_));       \
    }                                                                 \
  } while (0)
