# Threading Architecture

1. Main (UI) Thread:

   * *Function*: Manages the SDL2 window, OpenGL context, and the Lua
     environment.

   * *Responsibilities*: Processes user input (mouse/keyboard),
	 executes the Lua update() and draw() loops, renders the Waterfall
	 and Spectrum, and handles all File I/O (loading config, logging).

   * *Constraints*: Must never block on network or heavy DSP;
     otherwise, the UI "stutters."


2. DSP (Reception) Thread:

   * *Function*: High-priority thread managed by TwinConn that
     triggers upon arrival of network IQ frames.

   * *Responsibilities*: Performs frequency shifting (LO rotation),
	 Triple-QSD combination, LMS balancing, baseband filtering, and
	 demodulation.

   * *Constraints*: Strictly No-Blocking. No std::cout, no printf, no
	 File I/O, and absolutely no Lua calls. Communication with other
	 threads is done via thread-safe lock-free buffers (audioBuffer,
	 iqBuffer) or atomic variables.


3. Audio Callback Thread:

   * *Function*: System-managed thread that pulls samples from the
     application to feed the sound card (via SDL_Audio).

   * *Responsibilities*: Reads from the audioBuffer (populated by
     DSP), applies master volume, and mixes the optional sidetone.

   * *Constraints*: Must return quickly to prevent audio under-runs
     (clicks/pops).


4. Command (Worker) Thread:

   * *Function*: Asynchronous queue processor.

   * *Responsibilities*: Serializes outgoing control commands (Set
     VFO, Set Gain) to the hardware/twin over TCP.

   * *Constraints*: Prevents network latency from blocking the UI
     thread during property changes.

