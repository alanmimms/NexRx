#pragma once

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
  virtual void execute(uint64_t currentTime) = 0;
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
      uint64_t periodPS = 1ull * 1000ull * 1000ull * 1000ull * 1000ull / freqHz;
      halfPeriodPS = periodPS / 2;
    }

  uint64_t timeToNextEvent() const override { return nextEdgeTimePS; }

  void execute(uint64_t currentTime) override {

    if (currentTime >= nextEdgeTimePS) {
      *clkPin = !*clkPin;
      nextEdgeTimePS += halfPeriodPS;
    }
  }
};

struct TimeCondition {
  uint64_t wakeTime;		// Absolute time to next awaken
  bool isReady(uint64_t currentTime) const { return currentTime >= wakeTime; }
};


struct SignalCondition {
  CData *signalP;
  CData expectedValue;
  bool isReady(uint64_t /*currentTime*/) const { return *signalP == expectedValue; }
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
  void execute(uint64_t currentTime) override {
    std::vector<std::coroutine_handle<>> readyTasks;

    // Pass 1: Gather ready tasks and remove them from the waiting list
    for (auto it = waitingTasks.begin(); it != waitingTasks.end(); ) {

      bool ready = std::visit([currentTime](auto &&cond) {
	  return cond.isReady(currentTime);
	}, it->condition);

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
