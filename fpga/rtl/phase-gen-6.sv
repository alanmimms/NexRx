/**
 * phase-gen-6.sv
 * One-hot ring counter for 6-phase sextature clocks.
 */
module PhaseGen6 (
    input  logic       clk,
    input  logic       resetN,
    input  logic       pulse,
    output logic [5:0] phases
);

    logic [5:0] ring;

    always_ff @(posedge clk or negedge resetN) begin
        if (!resetN) begin
            ring <= 6'b000001;
        end else if (pulse) begin
            ring <= {ring[4:0], ring[5]};
        end
    end

    assign phases[0] = ring[0] | ring[5];
    assign phases[1] = ring[1] | ring[0];
    assign phases[2] = ring[2] | ring[1];
    assign phases[3] = ring[3] | ring[2];
    assign phases[4] = ring[4] | ring[3];
    assign phases[5] = ring[5] | ring[4];

endmodule
