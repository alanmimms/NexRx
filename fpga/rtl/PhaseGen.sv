/**
 * phaseGen.sv
 * One-hot ring counter for N-phase quadrature clocks.
 */
module PhaseGen
  #(parameter int N = 8)
  (
   input  logic       clk,
   input  logic       resetN,
   output logic [N-1:0] phases);

  timeunit 1ns;
  timeprecision 1ps;

  always_ff @(posedge clk or negedge resetN) begin

    if (!resetN) begin
      phases <= {{(N-1){1'b0}}, 1'b1};
    end else begin
      phases <= {phases[N-2:0], phases[N-1]};
    end
  end

endmodule
