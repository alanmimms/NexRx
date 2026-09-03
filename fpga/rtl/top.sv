/**
 * nexrx-top.sv
 * Top-level module for NexRx CPLDs.
 */
module top (
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

  timeunit 1ns;
  timeprecision 1ps;

  /* Clocking & Reset */
  logic clkSys;    // TCXO based Master Clock
  logic clkVFO;	   // Current "VFO" freq clock
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
  logic [63:0] tcxoTimer;
  RegAccess uReg (.clkSys(clkSys),
		  .resetN(resetN),
		  .spiSCK(spiSCK),
		  .spiMOSI(spiMOSI),
		  .spiMISO(spiMISO),
		  .spiNSS(spiNSS),
		  .tcxoTimer(tcxoTimer));

  //==================================================================
  // Precision Monotonic Timer
  //==================================================================
  Timer64Bit uTimer (.clk(clkSys),
		     .resetN(resetN),
		     .latchTrigger(1'b0),
		     .countLow(tcxoTimer[31:0]),
		     .countHigh(tcxoTimer[63:32]));

  logic [3:0] qsdClk;
  logic [7:0] osdClk;

  //==================================================================
  // Phase Generators (Walking Rings)
  //==================================================================
  PhaseGen #(.N(4)) phase4(.clk(clkSys), .resetN(resetN), .phases(qsdClk));
  PhaseGen #(.N(8)) phase8(.clk(clkSys), .resetN(resetN), .phases(osdClk));

endmodule
