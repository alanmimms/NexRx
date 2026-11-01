import CPLDRegs::*;

/**
 * nexrx-top.sv
 * Top-level module for NexRx CPLDs.
 */
module top (
	    input  logic clkTCXO,	// External TCXO
	    input  logic clkOSD,	// Si5351 generated "VFO" for phase outputs

	    /* SPI Slave Interface */
	    input  logic cpldSCK,
	    input  logic cpldMOSI,
	    output logic cpldMISO,
	    input  logic cpldNSS,

	    input  logic gnssPPS,	// GNSS 1pps signal for TCXO freq counter

	    // Output for each octature phase
	    output logic [7:0] nPh
	    );

`ifdef VERILATOR
  timeunit 1ns;
  timeprecision 1ps;
`endif

  /* Clocking & Reset */
  logic clkSys;    // TCXO based Master Clock

  // Build a Verilator-compatible reset from known FPGA traits.
  logic resetN;
  logic [3:0] porShift = 4'b0000;

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

  logic [31:0] shiftReg;
  logic [5:0]  bitCnt;
  logic [7:0]  cmdLatch;
  logic        cmdDone;

  logic [6:0]  addr;
  logic        isWrite;
  logic [31:0] dataIn;
  logic [31:0] dataOut;

  //==================================================================
  // SPI Frontend
  //==================================================================
  always_ff @(posedge cpldSCK or posedge cpldNSS) begin

    if (cpldNSS) begin
      bitCnt <= 6'd0;
      cmdDone <= 1'b0;
      cmdLatch <= 8'h0;
    end else begin

      if (bitCnt < 6'd8) begin
        cmdLatch <= {cmdLatch[6:0], cpldMOSI};
      end else begin
        shiftReg <= {shiftReg[30:0], cpldMOSI};
      end

      if (bitCnt == 6'd39) begin
        cmdDone <= 1'b1;
      end else begin
        bitCnt <= bitCnt + 1'b1;
      end
    end
  end

  always_ff @(negedge cpldSCK or posedge cpldNSS) begin

    if (cpldNSS) begin
      cpldMISO <= 1'b0;
    end else begin

      if (bitCnt >= 6'd8 && bitCnt < 6'd40) begin
        cpldMISO <= dataOut[39 - bitCnt];
      end else begin
        cpldMISO <= 1'b0;
      end
    end
  end

  assign addr = cmdLatch[6:0];
  assign isWrite = cmdLatch[7];
  assign dataIn = shiftReg;

  //==================================================================
  // Register Logic
  //==================================================================
  always_ff @(posedge clkSys) begin

    if (cmdDone) begin

      if (isWrite) begin

        case (addr)
	  aCPLDControl:	control <= dataIn;
	  default: ;
        endcase
      end
    end

    if (!resetN) control <= initCPLDControl;
  end

  always_comb begin
    case (addr)
      aCPLDControl:	dataOut = 32'(control);
      aCPLDPPSLatchHi:	dataOut = tcxoValue[63:32];
      aCPLDPPSLatchLo:	dataOut = tcxoValue[31:0];
      aCPLDSig:		dataOut = eSigValVal;	/* 'NxRx' */
      default:		dataOut = 'hDEADBEEF;
    endcase
  end

  //==================================================================
  // Precision frequency counter for clkSys gated by 1pps from GNSS.
  //==================================================================
  logic [63:0] tcxoCounter = 0;

  // Synchronizer to bring 1pps signal into TCXO clock domain safely.
  logic [2:0] ppsSync;
  wire ppsRise = ppsSync[1] & ~ppsSync[2];

  always_ff @(posedge clkSys) begin
    ppsSync <= {ppsSync[1:0], gnssPPS};
  end

  always_ff @(posedge clkSys) begin
    tcxoCounter <= tcxoCounter + 1'b1;

    if (ppsRise) begin
      tcxoValue <= tcxoCounter;
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
      nPh <= ~{'0, osdPhases[6], '0, osdPhases[4], '0, osdPhases[2], '0, osdPhases[0]};

    osdPhases <= {osdPhases[6:0], osdPhases[7]};
  end

endmodule
