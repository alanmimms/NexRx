/**
 * timer-64bit.sv
 * Monotonic 64-bit counter clocked by 40 MHz TCXO.
 */
module Timer64Bit (
		   input  logic        clk,
		   input  logic        resetN,
		   input  logic        latchTrigger,
		   output logic [31:0] countLow,
		   output logic [31:0] countHigh
		   );

  timeunit 1ns;
  timeprecision 1ps;

  logic [63:0] counter;
  logic [31:0] highLatch;

  always_ff @(posedge clk or negedge resetN) begin
    if (!resetN) begin
      counter <= 64'h0;
    end else begin
      counter <= counter + 1'b1;
    end
  end

  always_ff @(posedge clk or negedge resetN) begin
    if (!resetN) begin
      highLatch <= 32'h0;
    end else if (latchTrigger) begin
      highLatch <= counter[63:32];
    end
  end

  assign countLow = counter[31:0];
  assign countHigh = highLatch;

endmodule
