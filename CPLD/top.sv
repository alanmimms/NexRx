import CPLDRegs::*;

/**
 * nexrx-top.sv
 * Top-level module for teeny little NexRx ICE40LP384-SG32 CPLDs.
 */
module top (
	    input  logic clkTCXOpin,	// External TCXO
	    input  logic clkOSDpin,	// Si5351 generated "VFO" for phase outputs

	    /* SPI Slave Interface */
	    input  logic cpldSCLKpin,
	    input  logic cpldMOSIpin,
	    output logic cpldMISOpin,
	    input  logic cpldNSSpin,

	    input  logic gnssPPSpin,	// GNSS 1pps signal for TCXO freq counter

	    // Output for each octature phase
	    output logic [7:0] nPh
	    );

`ifdef VERILATOR
  timeunit 1ns;
  timeprecision 1ps;
`endif

  /* Clocking & Reset */
  logic clkSys;    // TCXO based Master Clock
  logic clkTCXO;   // Global buffered output clock from clkTCXOpin
  logic clkOSD;	   // Global buffered output clock from clkOSDpin

`ifdef VERILATOR
  assign clkTCXO = clkTCXOpin;
  assign clkOSD = clkOSDpin;
`else
  // Explicit global buffer instantiation for dedicated low latency
  // low jitter pin-to-global entry.
  SB_GB_IO #(
	     .PIN_TYPE(6'b000001)
	     ) tcxoClkBuffer (
			     .PACKAGE_PIN(clkTCXOpin),
			     .GLOBAL_BUFFER_OUTPUT(clkTCXO)
			     );

  SB_GB_IO #(
	     .PIN_TYPE(6'b000001)
	     ) osdClkBuffer (
			     .PACKAGE_PIN(clkOSDpin),
			     .GLOBAL_BUFFER_OUTPUT(clkOSD)
			     );
`endif

  // Build a Verilator-compatible reset from known FPGA traits.
  logic resetN;
  logic [3:0] porShift = 4'd0;

  assign resetN = &porShift;

  always_ff @(posedge clkTCXO) begin
    porShift <= {porShift[2:0], 1'b1};
  end

  // THIS PROBABLY NEEDS GLOBAL CLOCK BUFFER.
  assign clkSys = clkTCXO;

  //==================================================================
  // Register Bank & SPI
  //==================================================================
  CPLDRegs::tCPLDControl control;
  logic [63:0] tcxoValue;

  logic syncSCLK;
  logic sclkChanged;
  Synchronizer SCLKsyncizer(.clk(clkSys),
			    .i(cpldSCLKpin),
			    .o(syncSCLK),
			    .rising(sclkChanged));

  logic syncPPS;
  Synchronizer PPSsyncizer(.clk(clkSys),
			   .i(gnssPPSpin),
			   .o(syncPPS),
			   .rising());

  logic syncNSS;
  Synchronizer NSSsyncizer(.clk(clkSys),
			   .i(cpldNSSpin),
			   .o(syncNSS),
			   .rising());

  logic syncMOSI;
  Synchronizer MOSIsyncizer(.clk(clkSys),
			    .i(cpldMOSIpin),
			    .o(syncMOSI),
			    .rising());

  // This is the max width of a payload being transferred. It's also
  // used to accumulate the read/write and address bits at the start
  // of a transaction.
  logic [31:0] shiftReg;

  logic [5:0]  bitCnt = 0;

  logic [6:0]  addr = 0;
  logic        isWrite = 0;

  //==================================================================
  // SPI Frontend
  //==================================================================

  assign cpldMISOpin = shiftReg[31];

  logic [6:0] nuAddr;

  assign nuAddr = {addr[5:0], syncMOSI};

  // We use posedge cpldNSS as our reset.
  always_ff @(posedge syncSCLK or posedge syncNSS) begin

    if (syncNSS) begin
      bitCnt <= '0;
      shiftReg <= '0;
    end else begin

      // When we get to 8 bits, we have the read/write bit and seven
      // address bits. We know what we're reading or writing by this
      // point. This also tells us how many bit clocks will follow for
      // this address.
      if (bitCnt == 'd0) begin
	isWrite <= syncMOSI;
      end else if (bitCnt < 'd8) begin
	addr <= nuAddr;

	if (!isWrite) begin	// Register read path

	  case (nuAddr)
	    aCPLDControl:	shiftReg[7:0] <= control;
	    aCPLDTCXOLatch:	shiftReg <= {6'b0, tcxoCounter};
	    aCPLDSig:		shiftReg <= eSigValVal;
	    default:		shiftReg <= '0;
	  endcase
	end
      end else begin

	if (isWrite) begin
	  shiftReg <= {shiftReg[30:0], syncMOSI};
	end else begin
	  shiftReg <= {shiftReg[30:0], shiftReg[31]};
	end
      end

      if (isWrite) begin

	if (addr == aCPLDControl && bitCnt == wCPLDControl) control <= shiftReg[7:0];
	else if (addr == aCPLDSig && bitCnt == wCPLDSig) ;
	else if (addr == aCPLDTCXOLatch && bitCnt == wCPLDTCXOLatch && counterState != sCounterTCXO) begin
//	  tcxoCounter <= shiftReg[25:0];	This isn't strictly necessary and convolutes the code to do it safely
	end

      end

      bitCnt <= bitCnt + 1;
    end
  end

  //==================================================================
  // Precision frequency counter for clkTCXO gated by 1pps from GNSS.
  //==================================================================
  logic [25:0] tcxoCounter;
  typedef enum logic[1:0] {sCounterIdle, sCounterTCXO, sCounterFrozen, sCounterReadable} tCounterState;
  tCounterState counterState = sCounterIdle;


  // While counter is active, each posedge of clkTCXO increments tcxoCounter.
  always_ff @(posedge clkTCXO) begin

    case (counterState)
      sCounterIdle:
	if (syncPPS) counterState <= sCounterTCXO;

      sCounterTCXO:
	if (!syncPPS) counterState <= sCounterFrozen;

      sCounterFrozen:
	if (syncPPS) counterState <= sCounterReadable;

      sCounterReadable:
	;

    endcase

    if (counterState == sCounterTCXO) tcxoCounter <= tcxoCounter + 1;
  end

  //==================================================================
  // Phase Generators (Walking Rings)
  //==================================================================
  logic [7:0] osdPhases = 8'b10000000;

  always_ff @(negedge clkOSD) begin

    if (control.octMode)
      nPh <= ~osdPhases;
    else
      nPh <= ~{'0, osdPhases[6], '0, osdPhases[4], '0, osdPhases[2], '0, osdPhases[0]};

    osdPhases <= {osdPhases[6:0], osdPhases[7]};
  end

endmodule


module Synchronizer
  #(parameter int WIDTH=1)
  (
   input logic clk,
   input logic [WIDTH-1:0] i,
   output logic [WIDTH-1:0] o,
   output logic rising);

`ifdef VERILATOR
  timeunit 1ns;
  timeprecision 1ps;
`endif

  (* async_reg="true" *) logic [WIDTH-1:0] stage1;
  (* async_reg="true" *) logic [WIDTH-1:0] stage2;

  always_ff @(posedge clk) begin
    stage1 <= i;
    stage2 <= stage1;
  end

  assign rising = stage2 & !stage1;
  assign o = stage2;
endmodule // Synchronizer
