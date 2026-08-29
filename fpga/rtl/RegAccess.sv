import CPLDRegs::*;

/**
 * RegAccess.sv
 * SPI-accessible register map for NexRx CPLD.
 */
module RegAccess (
		  input  logic        clkSys,
		  input  logic        resetN,

		  /* SPI Interface */
		  input  logic        spiSCK,
		  input  logic        spiMOSI,
		  output logic        spiMISO,
		  input  logic        spiNSS,

		  /* Inputs from Internal Logic */
		  input  logic [63:0] tcxoTimer
		  );

  timeunit 1ns;
  timeprecision 1ps;

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
  always_ff @(posedge clkSys or negedge resetN) begin

    if (!resetN) begin
    end else begin

      if (cmdDone) begin

        if (isWrite) begin

          case (addr)
	    default: ;
          endcase
        end else if (addr == 7'h03) begin
          timeHLatch <= tcxoTimer[63:32];
        end
      end
    end
  end

  always_comb begin
    case (addr)
      7'h00: dataOut = 32'h4E585258; /* "NXRX" */
      7'h01: dataOut = 32'h00010000; /* v1.0.0 */
      7'h02: dataOut = tcxoTimer[31:0];
      7'h03: dataOut = timeHLatch;
      default: dataOut = 32'hDEADBEEF;
    endcase
  end

endmodule
