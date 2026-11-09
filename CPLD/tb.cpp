#include <iostream>
#include <cstdint>
#include <memory>
#include <iomanip>
#include <algorithm>
#include <coroutine>
#include <exception>
#include <stdexcept>
#include <variant>
#include <vector>
#include <sstream>
#include <string>

#include "Vtop.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "regs.h"
#include "hexout.h"
#include "simtask.h"
#include "testcase.h"


static Vtop* top;

static const char waveformFileName[] = "waveform.vcd";

static uint64_t currentTime;	// Current sim time in ps

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


// SPI clock 2MHz half period in ps.
static constexpr uint64_t spiHalfPeriodPS = 1000 * 1000;


static SimTask putBit(Vtop* top, unsigned bit) {
  top->cpldMOSIpin = bit;
  co_await WaitTime{spiHalfPeriodPS};
  top->cpldSCLKpin = 1;
  co_await WaitTime{spiHalfPeriodPS};
  top->cpldSCLKpin = 0;
  co_return;
}


static SimTask getBit(Vtop* top, unsigned &result) {
  top->cpldMOSIpin = 1;
  co_await WaitTime{spiHalfPeriodPS};
  top->cpldSCLKpin = 1;
  result = top->cpldMISOpin;
  co_await WaitTime{spiHalfPeriodPS};
  top->cpldSCLKpin = 0;
  co_return;
}


// A high-level, linear SPI driver coroutine
// A true Async SPI Master Coroutine
SimTask spiWrite(Vtop* top, unsigned addr, uint32_t data) {
  top->cpldNSSpin = 0;
  co_await WaitTime{spiHalfPeriodPS};
    
  // Send write bit and then address
  co_await putBit(top, 1);		// WRITE bit
  for (int k=6; k >= 0; --k) co_await putBit(top, (addr >> k) & 1);

  for (int k=31; k >= 0; --k) co_await putBit(top, (data >> k) & 1);

  co_await WaitTime{spiHalfPeriodPS};
  top->cpldNSSpin = 1;
  co_await WaitTime{spiHalfPeriodPS};

  co_return;
}


// A true Async SPI Read Coroutine
SimTask spiRead(Vtop* top, unsigned addr, uint32_t &dataOut) {
  top->cpldNSSpin = 0;
  co_await WaitTime{spiHalfPeriodPS};
    
  uint32_t readVal = 0;

  // Send read bit and then address
  co_await putBit(top, 0);		// READ bit
  for (int k=6; k >= 0; --k) co_await putBit(top, (addr >> k) & 1);

  for (int k=31; k >= 0; --k) {
    unsigned bit = 0;
    co_await getBit(top, bit);
    readVal = (readVal << 1) | bit;
  }

  dataOut = readVal;

  co_await WaitTime{spiHalfPeriodPS};
  top->cpldNSSpin = 1;
  co_await WaitTime{spiHalfPeriodPS};

  co_return;
}


// Read the hardware signature register to verify SPI read.
TEST_CASE("readCPLDSig") {
  uint32_t sig = 0;
  co_await spiRead(top, aCPLDSig, sig);
  std::cout << "SPI: Read Signature register (" << HexOut(aCPLDSig, 2) << "): "
	    << HexOut(sig) << std::endl << std::flush;
  if (sig != CPLDSig::eVal) {
    std::ostringstream msg;
    msg << "SPI ERROR: Signature mismatch! "
	<< "Expected " << HexOut(CPLDSig::eVal) << ", got " << HexOut(sig);
  }

  co_return;
}


// Freeze TCXO counter, write a pattern, read it back and verify.
TEST_CASE("readAndWriteCPLDCounter") {
  // Freeze the TCXO counter.
  CPLDControl freezer = {.octMode= 1, .counterThaw= 0, .counterState=CPLDControl::counterStateFrozen};
  co_await spiWrite(top, aCPLDControl, freezer.u);

  // Write then read back the TCXO counter while it's in frozen state to validate write path.
  const uint32_t counterTest = 0x12345678;
  co_await spiWrite(top, aCPLDCounter, counterTest);
  co_await WaitTime{NS_TO_PS(100)};	// Wait a bit to test case TCXO counting happening while frozen

  uint32_t counterVal = 0;
  co_await spiRead(top, aCPLDCounter, counterVal);

  std::cout << "SPI: Read Counter register: " << HexOut(counterVal) << std::endl << std::flush;
  if (counterVal == counterTest) {
    std::cout << "SPI: Counter value read matches - Success!" << std::endl << std::flush;
  } else {
    std::ostringstream msg;
    msg << "SPI ERROR: Counter value mismatch! "
	<< "Expected " << HexOut(counterTest) << ", got " << HexOut(counterVal);
    throw std::runtime_error(msg.str());
  }

  co_return;
}


// Verify TCXO counter is counting up properly in specified counter
// state.
static SimTask verifyCounterCountsInState(CPLDControl::CounterState state, Vtop *top) {
  // Zero the counter while it's frozen.
  co_await spiWrite(top, aCPLDCounter, 0);
  
  // Unfreeze the TCXO counter into state.
  CPLDControl counting1 = {.octMode= 1, .counterThaw= 0, .counterState=state};
  co_await spiWrite(top, aCPLDControl, counting1.u);
  co_await WaitTime{NS_TO_PS(10)};	// Delay for a few TCXO clocks.

  // Read counter now while it's counting.
  uint32_t counterVal = 0;
  co_await spiRead(top, aCPLDCounter, counterVal);
  std::cout << "SPI: Read Counter register: " << HexOut(counterVal) << std::endl << std::flush;

  if (counterVal == 0 || counterVal > 1000) {
    throw std::runtime_error("SPI ERROR: Counter value out of range! Expected !=0 and < 1000");
  }

  co_return;
}


TEST_CASE("verifyCounterCountsInCounting1") {
  co_await verifyCounterCountsInState(CPLDControl::counterStateCounting1, top);
  co_return;
}


TEST_CASE("verifyCounterCountsInCounting2") {
  co_await verifyCounterCountsInState(CPLDControl::counterStateCounting2, top);
  co_return;
}


static SimTask runTestSequence(Vtop* top) {
  // Wait 10ns before starting for luck.
  co_await WaitTime(10*1000);



 // Wait 10,000 TCXO cycles (assuming 40MHz clock = 25ns period)
  co_await WaitTime{10000 * NS_TO_PS(25)};
  co_return;
}


int main(int argc, char *argv[]) {
  top = new Vtop();
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
  auto runAllTask = TestRegistry::getInstance().runAll(top);

  while (!Verilated::gotFinish() && currentTime < maxSimTime) {
    uint64_t nextTime = UINT64_MAX;

    for (auto* source : sources) {
      nextTime = std::min(nextTime, source->timeToNextEvent());
    }

    currentTime = nextTime;

    // Fire events
    for (auto* source : sources) {
      if (source->timeToNextEvent() <= currentTime) source->execute(currentTime);
    }

    top->eval();
    theTM.execute(currentTime);
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
