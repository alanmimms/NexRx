#pragma once
#include <cstdio>
#include <cstdint>
#include "simtask.h"

// A simple test case declaration system. Use this like:
// TEST_CASE("Clock generation and reset sequence") {
//   co_await resetDevice();
//   for (int cycle = 0; cycle < 10; ++cycle) co_await stepClock();
//   co_return;
// }
constexpr uint64_t MEG(uint64_t m) { return m * 1000ull * 1000ull; }
constexpr uint64_t NS_TO_PS(uint64_t t) { return t * 1000ull; }
constexpr uint64_t US_TO_PS(uint64_t t) { return t * 1000ull * 1000ull; }
constexpr uint64_t MS_TO_PS(uint64_t t) { return t * 1000ull * 1000ull * 1000ull; }
constexpr uint64_t SEC_TO_PS(uint64_t t) { return t * 1000ull * 1000ull * 1000ull * 1000ull; }

using SimTestFunction = SimTask (*)(Vtop* top);

struct TestCaseEntry {
  const char* testName;
  SimTestFunction testFn;
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

  SimTask runAll(Vtop *top) {
    int failureCount = 0;
    int passCount = 0;
    std::printf("[==========] Running %d test cases.\n", entryCount);

    for (TestCaseEntry* current = headNode; current != nullptr; current = current->nextNode) {
      std::printf("[ RUN      ] %s\n", current->testName);

      try {
        // Instantiate and run each test case to completion
        SimTask testTask = current->testFn(top);
        co_await testTask;

        std::printf("[       OK ] %s\n", current->testName);
        passCount++;
      } catch (const std::exception& ex) {
        std::printf("[  FAILED  ] %s: %s\n", current->testName, ex.what());
        failureCount++;
      } catch (...) {
        std::printf("[  FAILED  ] %s: unknown exception\n", current->testName);
        failureCount++;
      }
    }

    std::printf("[==========] %d tests ran. %d passed, %d failed.\n", 
                entryCount, passCount, failureCount);
    co_return;
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
  TestAutoReg(TestCaseEntry* entry, const char* name, SimTestFunction fn) {
    entry->testName = name;
    entry->testFn = fn;
    entry->nextNode = nullptr;
    registerTestCase(entry);
  }
};


#define INTERNAL_CONCAT_IMPL(s1, s2) s1##s2
#define INTERNAL_CONCAT(s1, s2) INTERNAL_CONCAT_IMPL(s1, s2)

#ifdef __COUNTER__
#define INTERNAL_UNIQUE_ID __COUNTER__
#else
#define INTERNAL_UNIQUE_ID __LINE__
#endif

// Underlying helper that freezes the unique numeric ID across all declarations
#define TEST_CASE_IMPL(testName, id) \
  static SimTask INTERNAL_CONCAT(testCoro_, id)(Vtop* top); \
  static TestCaseEntry INTERNAL_CONCAT(testEntry_, id); \
  static const TestAutoReg INTERNAL_CONCAT(autoReg_, id)( \
    &INTERNAL_CONCAT(testEntry_, id), \
    testName, \
    &INTERNAL_CONCAT(testCoro_, id) \
  ); \
  static SimTask INTERNAL_CONCAT(testCoro_, id)(Vtop* top)

// Outer macro expanding INTERNAL_UNIQUE_ID exactly once
#define TEST_CASE(testName) TEST_CASE_IMPL(testName, INTERNAL_UNIQUE_ID)
