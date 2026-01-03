This document outlines the **NexRig Digital Twin** architecture. This environment allows for bit-identical firmware development, high-fidelity RF physics validation, and host-side DSP optimization before physical hardware exists.

---

# NexRig Digital Twin Architecture (NexRX Focus)

## 1. Executive Summary

The Digital Twin is a **Software-in-the-Loop (SiL)** simulation environment that combines a high-performance analog solver (Xyce) with a native-compiled instance of the Zephyr RTOS firmware. It treats the PC as the primary "Brain" (DSP/UI) and the simulated hardware as a high-bandwidth interleaved digitizer connected via a virtual 480 Mbps USB-HS link.

---

## 2. System Components

### 2.1 The Physics Engine (Xyce)

- **Role:** Simulates the analog RF signal path.
    
- **Implementation:** Linked as a shared library (`libxyce`) into the C++ Orchestrator.
    
- **Key Models:**
    
    - **Antenna Stimulus:** A C++ class that injects raw RF captures or synthetic multi-tone signals into the `Antenna_In` node.
        
    - **Preselector:** Models the 200Ω switched LC bank using DCR/ESR data from commercial SMD inductors.
        
    - **Pentafilar Transformer:** A mutual-inductance model ($K$-factors) representing the BN-43-202 core.
        
    - **Triple-QSD:** Time-varying resistors ($R_{on}/R_{off}$) representing the **TS3A4751** CMOS switches.
        

### 2.2 The FPGA Surrogate (NCO Engine)

- **Role:** Generates the high-speed switching logic for the mixers.
    
- **Implementation:** C++ class within the Orchestrator.
    
- **Logic:** A 32-bit phase accumulator updated at each Xyce time-step (720 MHz equivalent).
    
- **Output:** Drives the "Gate" nodes of the QSD switches in Xyce with precise phase offsets ($0^\circ, 120^\circ, 240^\circ$).
    

### 2.3 The Firmware Surrogate (Zephyr `native_sim`)

- **Role:** Runs the **actual** production C code for the STM32H7.
    
- **Implementation:** Compiled for x86_64 using Zephyr's Native Simulator platform.
    
- **Peripheral Abstraction:**
    
    - **I/O:** Redirects register writes (Relays, Attenuators) to the Virtual Register Map.
        
    - **DMA:** Simulated via a "Hardware Model" that pulls 6-channel interleaved samples from shared memory.
        

### 2.4 The Interconnect (Virtual USB-HS)

- **Control Pipe (RPC):** Transport-agnostic binary RPC (via Nanopb) running over a Unix Domain Socket. Provides "Callable API" access to hardware registers.
    
- **Data Pipe (UAC2):** High-speed 6-channel interleaved stream (3x I/Q) transferred via **POSIX Shared Memory**.
    

---

## 3. Data Flow & Interfaces

### 3.1 The 6-Channel Interleaved Frame

To maintain perfect phase alignment across all three QSDs, data is moved in atomic "Frames." Each frame represents a single sample point in time across the entire receiver.

|**Index**|**Channel**|**Source**|
|---|---|---|
|0|$I_A$|Mixer A (0° Phase)|
|1|$Q_A$|Mixer A (0° Phase)|
|2|$I_B$|Mixer B (120° Phase)|
|3|$Q_B$|Mixer B (120° Phase)|
|4|$I_C$|Mixer C (240° Phase)|
|5|$Q_C$|Mixer C (240° Phase)|

### 3.2 Control Register Map (Protobuf)

The Host PC controls the simulated hardware via a `.proto` defined schema.

- `REG_VFO_FREQ`: Updates the C++ NCO Engine.
    
- `REG_PRESEL_MASK`: Updates Xyce component values (relays).
    
- `REG_ATTEN_DB`: Adjusts the simulated attenuator ladder.
    

---

## 4. Operational Workflow

1. **Orchestrator Initialization:** The C++ main program loads the Xyce netlist and starts the Zephyr `native_sim` process.
    
2. **The Heartbeat Loop:** * The Orchestrator queries the NCO Engine for the current switch states.
    
    - Xyce "Steps" forward in time (e.g., 1ns increments).
        
    - At the "ADC Sample" interval (1/192kHz), 6 voltage values are pulled from Xyce.
        
3. **Firmware Processing:** The "Virtual STM32" (Zephyr) picks up these samples from shared memory and encapsulates them for the Host PC.
    
4. **Host DSP:** The Host PC C++ application receives the interleaved stream and performs the 1-2-1 weighting, filtering, and demodulation.
    

---

## 5. Future Expansion: Transmitter Path

> **[RESERVED FOR TX IMPLEMENTATION]**
> 
> - _Planned Integration:_ 8-FET Segmented H-Bridge Model.
>     
> - _Planned Integration:_ 1-2-1 Waveform Synthesis Logic (FPGA Side).
>     
> - _Planned Integration:_ 200Ω Guanella Combiner and Filter Unit interactions.
>     

---

## 6. Implementation Notes

- **Consistency:** Use the same `.proto` file for Nanopb (Zephyr) and standard Protobuf (Host PC).
    
- **Determinism:** The simulation can be "Paused" at any point, allowing for inspection of analog voltages in Xyce and firmware state in Zephyr simultaneously.
    
- **Performance:** Xyce runs in a separate thread/process to utilize multi-core CPUs; the Orchestrator acts as the high-speed synchronizer.