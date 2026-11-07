#include "Vtop.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <iostream>
#include <cstdint>
#include "regs.h"
#include <memory>
#include <iomanip>
#include <algorithm>
#include <coroutine>
#include <exception>
#include <variant>
#include <vector>

constexpr uint64_t MEG(uint64_t m) { return m * 1000ull * 1000ull; }
constexpr uint64_t SEC_TO_PS(uint64_t t) { return t * 1000ull * 1000ull * 1000ull * 1000ull; }

static const char waveformFileName[] = "waveform.vcd";

static uint64_t currentTime;	// Current sim time in ps

struct [[nodiscard]] SimTask {

  struct promise_type {
    // Saves parent coroutine that called co_await on us
    std::coroutine_handle<> continuation = nullptr;

    SimTask get_return_object() {
      return SimTask{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
        
    // Start running immediately upon creation until the first wait
    std::suspend_never initial_suspend() noexcept { return {}; }
        
    // The magic happens here: When this coroutine finishes, wake up the parent!
    auto final_suspend() noexcept {

      struct FinalAwaiter {
	bool await_ready() noexcept { return false; }

	std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
	  // If a parent is waiting for us, return their handle to resume them instantly
	  if (h.promise().continuation) return h.promise().continuation;

	  // Otherwise, just yield back to the main simulation loop
	  return std::noop_coroutine();
	}

	void await_resume() noexcept {}
      };
      return FinalAwaiter{};
    }
        
    void return_void() {}
    void unhandled_exception() { std::terminate(); }
  };

  std::coroutine_handle<promise_type> handle;

  SimTask(std::coroutine_handle<promise_type> h) : handle(h) {}
    
  // --- Rule of 5: Safe C++ Memory Management to prevent memory leaks ---
  SimTask(const SimTask&) = delete;
  SimTask& operator=(const SimTask&) = delete;
  SimTask(SimTask&& other) noexcept : handle(other.handle) { other.handle = nullptr; }

  SimTask& operator=(SimTask&& other) noexcept {
    if (this != &other) {
      if (handle) handle.destroy();
      handle = other.handle;
      other.handle = nullptr;
    }
    return *this;
  }

  ~SimTask() { if (handle) handle.destroy(); }

  // Is the child already finished before we even tried to wait?
  bool await_ready() const noexcept { return !handle || handle.done(); }

  // Put the parent to sleep, and tell the child who the parent is.
  void await_suspend(std::coroutine_handle<> caller) noexcept {
    handle.promise().continuation = caller;
  }

  void await_resume() const noexcept {}
};


class EventSource {
public:
  virtual ~EventSource() = default;

  // Returns sim time (in ps) of the next event or UINT64_MAX if there
  // are no pending events for this source.
  virtual uint64_t timeToNextEvent() const = 0;

  // Called by the main loop when sim time reaches this event's time.
  virtual void execute() = 0;
};

class ClockSource : public EventSource {
private:
  uint64_t halfPeriodPS;
  uint64_t nextEdgeTimePS;
  CData* clkPin;

public:
  ClockSource(uint64_t freqHz, CData* pin) 
    : clkPin(pin), nextEdgeTimePS(0) 
  {
    uint64_t periodPS = SEC_TO_PS(1) / freqHz;
    halfPeriodPS = periodPS / 2;
  }

  uint64_t timeToNextEvent() const override { return nextEdgeTimePS; }

  void execute() override {

    if (currentTime >= nextEdgeTimePS) {
      *clkPin = !*clkPin;
      nextEdgeTimePS += halfPeriodPS;
    }
  }
};

struct TimeCondition {
  uint64_t wakeTime;		// Absolute time to next awaken
  bool isReady() const { return currentTime >= wakeTime; }
};


struct SignalCondition {
  CData *signalP;
  CData expectedValue;
  bool isReady() const { return *signalP == expectedValue; }
};


class TaskManager : public EventSource {
public:

  struct Waiter {
    std::coroutine_handle<> handle;
    std::variant<TimeCondition, SignalCondition> condition;
  };

  std::vector<Waiter> waitingTasks;

  template <typename ConditionType>
  void waitFor(std::coroutine_handle<> h, ConditionType cond) {
    waitingTasks.push_back({h, cond});
  }

  // --- REGISTRATION API ---
  // These are called by the Awaitables to push tasks into the list
  void waitForTime(std::coroutine_handle<> h, uint64_t wakeTime) {
    waitingTasks.push_back({h, TimeCondition{wakeTime}});
  }

  void waitForSignal(std::coroutine_handle<> h, CData* signal, CData value) {
    waitingTasks.push_back({h, SignalCondition{signal, value}});
  }

  uint64_t timeToNextEvent() const override {
    return UINT64_MAX; 
  }

  // 2-Pass Execution Pattern - called once to do sim loop.
  void execute() override {
    std::vector<std::coroutine_handle<>> readyTasks;

    // Pass 1: Gather ready tasks and remove them from the waiting list
    for (auto it = waitingTasks.begin(); it != waitingTasks.end(); ) {
      bool ready = std::visit([](auto &&cond) { return cond.isReady(); }, it->condition);

      if (ready) {
	readyTasks.push_back(it->handle);
	it = waitingTasks.erase(it); // Returns new iterator
      } else {
	++it; // Manually increment if no erase
      }
    }

    // Pass 2: Resume the ready tasks
    for (auto h: readyTasks) {
      if (h && !h.done()) h.resume();
    }
  }
};

static TaskManager theTM;


struct WaitEdge {
  CData* signal;
  CData targetValue;

  bool await_ready() const { return *signal == targetValue; }
  void await_suspend(std::coroutine_handle<> h) { theTM.waitForSignal(h, signal, targetValue); }
  void await_resume() const {}
};

struct WaitTime {
  uint64_t delayPS;

  bool await_ready() const { return delayPS == 0; }
  void await_suspend(std::coroutine_handle<> h) { theTM.waitForTime(h, currentTime + delayPS); }
  void await_resume() const {}
};


static unsigned getBitWidthForAddr(unsigned addr) {
  switch (addr) {
  case aCPLDControl:	return CPLDControl::bitW;
  case aCPLDTCXOLatch:	return CPLDTCXOLatch::bitW;
  case aCPLDSig:	return CPLDSig::bitW;

  default:
    std::cerr << "spiRead or spiWrite for unknown address!" << std::endl;
    return 0;
  }
}


// SPI clock 2MHz half period in ps.
static constexpr uint64_t spiHalfPeriodPS = 1000 * 1000;


static SimTask putBit(Vtop* top, unsigned bit) {
  top->cpldMOSIpin = bit;
  co_await WaitTime{spiHalfPeriodPS};
  top->cpldSCLKpin = 1;
  co_await WaitTime{spiHalfPeriodPS};
  top->cpldSCLKpin = 0;
}


static SimTask getBit(Vtop* top, unsigned &result) {
  top->cpldMOSIpin = 1;
  co_await WaitTime{spiHalfPeriodPS};
  top->cpldSCLKpin = 1;
  result = top->cpldMISOpin;
  co_await WaitTime{spiHalfPeriodPS};
  top->cpldSCLKpin = 0;
}


// A high-level, linear SPI driver coroutine
// A true Async SPI Master Coroutine
SimTask spiWrite(Vtop* top, uint8_t addr, uint32_t data) {
  top->cpldNSSpin = 0;
  top->cpldSCLKpin = 0;
  co_await WaitTime{spiHalfPeriodPS};
    
  // Send write bit and then address
  co_await putBit(top, 1);		// WRITE bit
  for (int k=6; k >= 0; --k) co_await putBit(top, (addr >> k) & 1);

  unsigned bitW = getBitWidthForAddr(addr);
  for (int k=bitW-1; k >= 0; --k) co_await putBit(top, (data >> k) & 1);

  co_await WaitTime{spiHalfPeriodPS};
  top->cpldNSSpin = 1;
  co_await WaitTime{spiHalfPeriodPS};
}


// A true Async SPI Read Coroutine
SimTask spiRead(Vtop* top, uint8_t addr, uint32_t &dataOut) {
  std::cout << "spiRead 0x" << std::hex << std::setw(2) << std::setfill('0') << (unsigned) addr
	    << std::endl << std::flush;
  top->cpldNSSpin = 0;
  top->cpldSCLKpin = 0;
  co_await WaitTime{spiHalfPeriodPS};
    
  uint32_t readVal = 0;

  // Send read bit and then address
  co_await putBit(top, 0);		// READ bit
  for (int k=6; k >= 0; --k) co_await putBit(top, (addr >> k) & 1);

  unsigned bitW = getBitWidthForAddr(addr);

  for (int k=bitW-1; k >= 0; --k) {
    unsigned bit = 0;
    co_await getBit(top, bit);
    readVal = (readVal << 1) | bit;
  }

  dataOut = readVal;

  co_await WaitTime{spiHalfPeriodPS};
  top->cpldNSSpin = 1;
  co_await WaitTime{spiHalfPeriodPS};
}


static SimTask runTestSequence(Vtop* top) {
  std::cout << "runTestSequence" << std::endl << std::flush;

  // Wait 100ns before starting for luck.
  co_await WaitTime(MEG(1)/10);

  // Read Hardware Signature register to verify SPI read
  uint32_t sig = 0;
  co_await spiRead(top, aCPLDSig, sig);
  std::cout << "SPI: Read Signature register (0x0F): 0x" 
            << std::hex << std::setw(8) << std::setfill('0') << sig << std::dec << std::endl << std::flush;
  if (sig == 0x4E785278) {
    std::cout << "SPI: Signature matches 'NxRx' (0x4E785278) - Success!" << std::endl << std::flush;
  } else {
    std::cout << "SPI ERROR: Signature mismatch! Expected 0x4E785278, got 0x" 
              << std::hex << sig << std::dec << std::endl << std::flush;
  }

  // Wait 10,000 TCXO cycles (assuming 40MHz clock = 25ns period)
  co_await WaitTime{10000 * 25000};
}


int main(int argc, char *argv[]) {
  Vtop* top = new Vtop();
  VerilatedVcdC* traceP = nullptr;
  
  Verilated::commandArgs(argc, argv);
  bool enableTrace = true;
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--notrace") { enableTrace = false; }
  }

  if (enableTrace) {
    traceP = new VerilatedVcdC;
    Verilated::traceEverOn(true);
    top->trace(traceP, 99);
    traceP->open(waveformFileName);
  }

  ClockSource clkTCXO(MEG(40), &top->clkTCXOpin);
  ClockSource clkOSD(MEG(5), &top->clkOSDpin);

  // This is faked to be 100kHz clock instead of 1Hz to test freq
  // counter in our lifetime.
  ClockSource gnssPPS(MEG(1)/10, &top->gnssPPSpin);

  // Initialize all driving pins
  top->clkTCXOpin = 0;
  top->clkOSDpin = 0;
  top->cpldNSSpin = 1;
  top->cpldSCLKpin = 0;
  top->cpldMOSIpin = 0;
  top->gnssPPSpin = 0;

  std::cout << "Starting simulation..." << std::endl << std::flush;

  std::vector<EventSource*> sources = {&clkTCXO, &clkOSD, &gnssPPS, &theTM};
  const uint64_t maxSimTime = MEG(10000); // 10ms to prevent infinite loops

  // Set up coroutine that drives our test sequence.
  auto testSeqTask = runTestSequence(top);

  while (!Verilated::gotFinish() && currentTime < maxSimTime) {
    uint64_t nextTime = UINT64_MAX;

    for (auto* source : sources) {
      nextTime = std::min(nextTime, source->timeToNextEvent());
    }

    currentTime = nextTime;

    // Fire events
    for (auto* source : sources) {
      if (source->timeToNextEvent() <= currentTime) source->execute();
    }

    top->eval();
    theTM.execute();
    top->eval(); 

    if (traceP) traceP->dump(currentTime);
  }

  if (traceP) {
    traceP->close();
    delete traceP;
  }

  std::cout << "=== Simulation Summary ===" << std::endl;
  std::cout << "Simulation time: " << (currentTime / 1000000ull) << " us" << std::endl;

  delete top;
  std::cout << "Simulation finished. Waveform saved." << std::endl;
  return 0;
}
