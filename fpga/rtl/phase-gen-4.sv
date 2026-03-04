/**
 * phase-gen-4.sv
 * One-hot ring counter for 4-phase quadrature clocks.
 */
module PhaseGen4 (
    input  logic       clk,
    input  logic       resetN,
    input  logic       pulse,
    output logic [3:0] phases
);

    logic [3:0] ring;

    always_ff @(posedge clk or negedge resetN) begin
        if (!resetN) begin
            ring <= 4'b0001;
        end else if (pulse) begin
            ring <= {ring[2:0], ring[3]};
        end
    end

    assign phases = ring;

endmodule
