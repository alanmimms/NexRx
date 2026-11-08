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
  logic [3:0] porShift = '0;

  assign resetN = &porShift;

  always_ff @(posedge clkTCXO) begin
    porShift <= {porShift[2:0], 1'b1};
  end

  // THIS PROBABLY NEEDS GLOBAL CLOCK BUFFER.
  assign clkSys = clkTCXO;

  //==================================================================
  // Register Bank & SPI
  //==================================================================
  CPLDRegs::tCPLDControl control = {28'h0, counterStateFrozen, 1'b0, 1'b1};
  logic [31:0] counter = initCPLDCounter;

  /* verilator lint_off PINMISSING */
  logic syncSCLK;
  logic sclkRising;
  Synchronizer SCLKsyzer(.clk(clkSys),
			 .i(cpldSCLKpin),
			 .o(syncSCLK),
			 .rising(sclkRising));

  logic syncPPS;
  Synchronizer PPSsyzer(.clk(clkSys),
			.i(gnssPPSpin),
			.o(syncPPS));

  logic syncNSS;
  logic nssRising;
  Synchronizer NSSsyzer(.clk(clkSys),
			.i(cpldNSSpin),
			.o(syncNSS),
			.rising(nssRising));

  logic syncMOSI;
  Synchronizer MOSIsyzer(.clk(clkSys),
			 .i(cpldMOSIpin),
			 .o(syncMOSI));
  /* verilator lint_on PINMISSING */

  logic [31:0] shiftReg;
  assign cpldMISOpin = shiftReg[31];
  logic [5:0]  bitCnt = 0;
  logic isWrite;
  logic [6:0]  addr;

  // This allows us - on the last bit of addr - to know its value.
  logic [6:0] nextAddr;
  assign nextAddr = {addr[5:0], syncMOSI};

  always_ff @(posedge clkTCXO) begin

    //==================================================================
    // SPI Frontend
    //==================================================================
    if (nssRising) begin       // We use posedge cpldNSS as our reset.

      if (bitCnt == 40) begin

	if (isWrite) begin

	  // CPLD WRITE.
	  case (addr)
	    aCPLDControl:	control <= shiftReg;
	    aCPLDCounter:	counter <= shiftReg;
	    aCPLDSig:		;
	    default:		;
	  endcase
	end
      end

      bitCnt <= 0;
      shiftReg <= 0;
      isWrite <= 0;
      addr <= 0;
    end else if (!syncNSS && sclkRising) begin

      if (bitCnt < 8) addr <= nextAddr;

      if (bitCnt == 0) begin
	isWrite <= syncMOSI;
      end else if (bitCnt < 8) begin

	if (!isWrite && bitCnt == 7) begin

	  // CPLD READ. We do this one bit earlier than "usual" so
	  // it's ready to serialize out on next SPI clock.
	  case (nextAddr)
	    aCPLDControl:	shiftReg <= control;
	    aCPLDCounter:	shiftReg <= counter;
	    aCPLDSig:		shiftReg <= sigVal;
	    default:		shiftReg <= '0;
	  endcase
	end
      end

      if (bitCnt >= 8 && bitCnt < 40) begin		// We're serializing the payload part of transaction

	if (isWrite) begin
	  shiftReg <= {shiftReg[30:0], syncMOSI};
	end else begin
	  shiftReg <= {shiftReg[30:0], 1'b0};
	end
      end

      bitCnt <= bitCnt + 1;
    end

    //==================================================================
    // Precision frequency counter for clkTCXO gated by 1pps from GNSS.
    //==================================================================
    case (control.counterState)
      counterStateWaiting:
	if (syncPPS) control.counterState <= counterStateCounting1;

      counterStateCounting1:
	if (!syncPPS) control.counterState <= counterStateCounting2;

      counterStateCounting2:
	if (syncPPS) control.counterState <= counterStateFrozen;

      counterStateFrozen:

	if (control.counterThaw) begin
	  control.counterState <= counterStateWaiting;
	  control.counterThaw <= 0;
	end
    endcase

    if (control.counterState == counterStateCounting1 || control.counterState == counterStateCounting2) begin
      counter <= counter + 1;
    end
  end

  //==================================================================
  // Phase Generators (Walking Rings)
  //==================================================================
  logic [7:0] osdPhases = 8'b10000000;

  always_ff @(negedge clkOSD) begin

    if (control.octMode)
      nPh <= ~osdPhases;
    else
      nPh <= ~{1'b0, osdPhases[6], 1'b0, osdPhases[4], 1'b0, osdPhases[2], 1'b0, osdPhases[0]};

    osdPhases <= {osdPhases[6:0], osdPhases[7]};
  end
endmodule


module Synchronizer
  #(parameter int WIDTH=1)
  (
   input logic clk,
   input logic [WIDTH-1:0] i,
   output logic [WIDTH-1:0] o,
   output logic rising,
   output logic falling);

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

  assign rising =  stage2 & !stage1;
  assign falling = !stage2 & stage1;
  assign o = stage2;
endmodule // Synchronizer
