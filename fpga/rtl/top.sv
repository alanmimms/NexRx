/**
 * nexrx-top.sv
 * Top-level module for NexRx CPLDs.
 */
`timescale 1ns/1ps

module NexRxTop (
		 input  logic iClk40M,    // 40 MHz External TCXO
		 input  logic iClkVFO,	 // Si5351 generated "VFO" for phase outputs
  
		 /* SPI Slave Interface */
		 input  logic spiSCK,
		 input  logic spiMOSI,
		 output logic spiMISO,
		 input  logic spiNSS,

		 input  logic iGNSS1PPS, // GNSS 1pps signal for TCXO counting

		 output logic [7:0] oPhase // Output for each octature phase
		 );

  /* Clocking & Reset */
  logic clkSys;    /* TCXO based Master Clock */
  logic resetN;

`ifdef SIMULATION
  initial clkSys = 0;
  always #25.0 clkSys = ~clkSys;
`else
  // THIS PROBABLY NEEDS GLOBAL CLOCK BUFFER.
  assign clkSys = iClk40M;
  assign clkVFO = iClkVFO;
`endif

  //==================================================================
  // Register Bank & SPI
  //==================================================================
  RegisterBank uRegBank (
			 .clkSys(clkSys),
			 .resetN(resetN),
			 .spiSck(spiSCK),
			 .spiMosi(spiMOSI),
			 .spiMiso(spiMISO),
			 .spiNss(spiNSS),
			 .ISGInc(ISGInc),
			 .QSD0Inc(QSD0Inc),
			 .QSD1Inc(QSD1Inc),
			 .QSD2Inc(QSD2Inc),
			 .commitFreq(commitFreq),
			 .commitPhase(commitPhase),
			 .tcxoTimer(tcxoTimer)
			 );

  //==================================================================
  // Noise Generator
  //==================================================================
  NoiseGen uNoise (.clk(clkSys), .resetN(resetN), .out(noiseBit));

  //==================================================================
  // Precision Monotonic Timer
  //==================================================================
  Timer64Bit uTimer (
		     .clk40M(clk40M),
		     .resetN(resetN),
		     .latchTrigger(1'b0),
		     .countLow(tcxoTimer[31:0]),
		     .countHigh(tcxoTimer[63:32])
		     );

  //==================================================================
  // NCO Cores
  //==================================================================
  NCOCore uNCOIsg  (.clk(clkSys), .resetN(resetN), .nextIncrement(ISGInc),  .commit(commitFreq), .forceReset(commitPhase), .pulse(ISGPulse));
  NCOCore uNCOQSD0 (.clk(clkSys), .resetN(resetN), .nextIncrement(QSD0Inc), .commit(commitFreq), .forceReset(commitPhase), .pulse(QSD0Pulse));
  NCOCore uNCOQSD1 (.clk(clkSys), .resetN(resetN), .nextIncrement(QSD1Inc), .commit(commitFreq), .forceReset(commitPhase), .pulse(QSD1Pulse));
  NCOCore uNCOQSD2 (.clk(clkSys), .resetN(resetN), .nextIncrement(QSD2Inc), .commit(commitFreq), .forceReset(commitPhase), .pulse(QSD2Pulse));

  //==================================================================
  // Phase Generators (Walking Rings)
  //==================================================================
  PhaseGen4 uGenQSD0 (.clk(clkSys), .resetN(resetN), .pulse(QSD0Pulse), .phases(QSD0Clk));
  PhaseGen4 uGenQSD1 (.clk(clkSys), .resetN(resetN), .pulse(QSD1Pulse), .phases(QSD1Clk));
  PhaseGen6 uGenQSD2 (.clk(clkSys), .resetN(resetN), .pulse(QSD2Pulse), .phases(QSD2Clk));

  //==================================================================
  // ISG Logic & Mode Mux
  //==================================================================
  always_ff @(posedge clkSys or negedge resetN) begin
    if (!resetN)
      ISGTone <= 1'b0;
    else if (ISGPulse)
      ISGTone <= ~ISGTone;
  end

  always_comb begin
    if (ISGInc == 32'd0)
      ISGOut = 1'b0;
    else if (ISGInc == 32'd1)
      ISGOut = noiseBit;
    else
      ISGOut = ISGTone;
  end

endmodule
