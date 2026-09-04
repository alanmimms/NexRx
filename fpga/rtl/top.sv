import CPLDRegs::*;

/**
 * nexrx-top.sv
 * Top-level module for NexRx CPLDs.
 */
module top (
	    input  logic clkTCXO,	// External TCXO
	    input  logic clkSynth,	// Si5351 generated "VFO" for phase outputs

	    /* SPI Slave Interface */
	    input  logic spiSCK,
	    input  logic spiMOSI,
	    output logic spiMISO,
	    input  logic spiNSS,

	    input  logic gnssPPS,	// GNSS 1pps signal for TCXO counting

	    // Output for each octature phase
	    output logic [7:0] nPh
	    );

  timeunit 1ns;
  timeprecision 1ps;

  /* Clocking & Reset */
  logic clkSys;    // TCXO based Master Clock
  logic clkVFO;	   // Current "VFO" freq clock
  logic resetN;

  // THIS PROBABLY NEEDS GLOBAL CLOCK BUFFER.
  assign clkSys = clkTCXO;
  assign clkVFO = clkSynth;

  initial resetN = 1;
  initial control = initCPLDControl;

  //==================================================================
  // Register Bank & SPI
  //==================================================================
  tCPLDControl control;
  logic [63:0] tcxoCounter;

  logic [31:0] shiftReg;
  logic [5:0]  bitCnt;
  logic [7:0]  cmdLatch;
  logic        cmdDone;

  logic [6:0]  addr;
  logic        isWrite;
  logic [31:0] dataIn;
  logic [31:0] dataOut;

  logic [31:0] timeHLatch;

  //==================================================================
  // SPI Frontend
  //==================================================================
  always_ff @(posedge spiSCK or posedge spiNSS) begin

    if (spiNSS) begin
      bitCnt <= 6'd0;
      cmdDone <= 1'b0;
      cmdLatch <= 8'h0;
    end else begin

      if (bitCnt < 6'd8) begin
        cmdLatch <= {cmdLatch[6:0], spiMOSI};
      end else begin
        shiftReg <= {shiftReg[30:0], spiMOSI};
      end

      if (bitCnt == 6'd39) begin
        cmdDone <= 1'b1;
      end else begin
        bitCnt <= bitCnt + 1'b1;
      end
    end
  end

  always_ff @(negedge spiSCK or posedge spiNSS) begin

    if (spiNSS) begin
      spiMISO <= 1'b0;
    end else begin

      if (bitCnt >= 6'd8 && bitCnt < 6'd40) begin
        spiMISO <= dataOut[39 - bitCnt];
      end else begin
        spiMISO <= 1'b0;
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
	  default: ;
        endcase
      end
    end
  end

  always_comb begin
    case (addr)
      aCPLDControl:	dataOut = 32'(control);
      aCPLDPPSLatchHi:	dataOut = tcxoCounter[63:32];
      aCPLDPPSLatchLo:	dataOut = tcxoCounter[31:0];
      aCPLDSig:		dataOut = eSigValVal;	/* 'NxRx' */
      default:		dataOut = 'hDEADBEEF;
    endcase
  end

  //==================================================================
  // Precision Monotonic Timer
  //==================================================================
  initial tcxoCounter = 0;

  always_ff @(posedge clkSys) begin
    tcxoCounter <= tcxoCounter + '1;
  end

  //==================================================================
  // Phase Generators (Walking Rings)
  //==================================================================
  logic [7:0] osdPhases;

  initial osdPhases = 8'b10000000;

  always_ff @(negedge clkVFO) begin

    if (control.octMode)
      nPh <= ~osdPhases;
    else
      nPh <= ~{'0, osdPhases[6], '0, osdPhases[4], '0, osdPhases[2], '0, osdPhases[0]};

    osdPhases <= {osdPhases[6:0], osdPhases[7]};
  end

endmodule
