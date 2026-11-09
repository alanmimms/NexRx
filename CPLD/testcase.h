#pragma once
#include <cstdio>
#include <cstdint>

constexpr uint64_t MEG(uint64_t m) { return m * 1000ull * 1000ull; }
constexpr uint64_t NS_TO_PS(uint64_t t) { return t * 1000ull; }
constexpr uint64_t US_TO_PS(uint64_t t) { return t * 1000ull * 1000ull; }
constexpr uint64_t MS_TO_PS(uint64_t t) { return t * 1000ull * 1000ull * 1000ull; }
constexpr uint64_t SEC_TO_PS(uint64_t t) { return t * 1000ull * 1000ull * 1000ull * 1000ull; }

using TestFunction = void (*)();

struct TestCaseEntry {
  const char* testName;
  TestFunction testFn;
  TestCaseEntry* nextNode;
};

class TestRegistry {
public:
  static TestRegistry& getInstance() {
    static TestRegistry instance;
    return instance;
  }

  void addTest(TestCaseEntry* entry) {
    if (!headNode) {
      headNode = entry;
      tailNode = entry;
    } else {
      tailNode->nextNode = entry;
      tailNode = entry;
    }
    tailNode->nextNode = nullptr;
    entryCount++;
  }

  int runAll() {
    int failureCount = 0;
    int passCount = 0;
    std::printf("[==========] Running %d test cases.\n", entryCount);

    for (TestCaseEntry* current = headNode; current != nullptr; current = current->nextNode) {
      std::printf("[ RUN      ] %s\n", current->testName);
      
      try {
        current->testFn();
        std::printf("[       OK ] %s\n", current->testName);
        passCount++;
      } catch (...) {
        std::printf("[  FAILED  ] %s (unhandled exception)\n", current->testName);
        failureCount++;
      }
    }

    std::printf("[==========] %d tests ran. %d passed, %d failed.\n", 
                entryCount, passCount, failureCount);
    return failureCount;
  }

  int getCount() const { return entryCount; }

private:
  TestRegistry() = default;
  TestCaseEntry* headNode = nullptr;
  TestCaseEntry* tailNode = nullptr;
  int entryCount = 0;
};

// C-linkage-friendly API wrappers
void registerTestCase(TestCaseEntry* entry) {
  TestRegistry::getInstance().addTest(entry);
}

struct TestAutoReg {
  TestAutoReg(TestCaseEntry* entry, const char* name, TestFunction fn) {
    entry->testName = name;
    entry->testFn = fn;
    entry->nextNode = nullptr;
    registerTestCase(entry);
  }
};


#define INTERNAL_CONCAT_IMPL(s1, s2) s1##s2
#define INTERNAL_CONCAT(s1, s2) INTERNAL_CONCAT_IMPL(s1, s2)

#ifdef __COUNTER__
#define INTERNAL_ANON_ID(base) INTERNAL_CONCAT(base, __COUNTER__)
#else
#define INTERNAL_ANON_ID(base) INTERNAL_CONCAT(base, __LINE__)
#endif

// A simple test case declaration system. Use this like:
// TEST_CASE("Clock generation and reset sequence") {
//   co_await resetDevice();
//   for (int cycle = 0; cycle < 10; ++cycle) co_await stepClock();
//   co_return;
// }(); }
#define TEST_CASE(testName, ...) \
  static void INTERNAL_ANON_ID(testWrapper_)(); \
  static TestCaseEntry INTERNAL_ANON_ID(testEntry_); \
  static const TestAutoReg INTERNAL_ANON_ID(autoReg_)( \
    &INTERNAL_ANON_ID(testEntry_), testName, &INTERNAL_ANON_ID(testWrapper_)); \
  static void INTERNAL_ANON_ID(testWrapper_)() { \
    driveSimulationToCompletion([]() -> SimTask { \
      __VA_ARGS__ \
    }()); \
  }
