/**
 * noise-gen.sv
 * 32-bit Galois LFSR for pseudo-random white noise generation.
 */
module NoiseGen (
    input  logic clk,
    input  logic resetN,
    output logic out
);

    logic [31:0] state;

    always_ff @(posedge clk or negedge resetN) begin
        if (!resetN) begin
            state <= 32'hACE1; /* Non-zero seed */
        end else begin
            /* Galois LFSR polynomial for 32 bits: x^32 + x^22 + x^2 + x^1 + 1 */
            state <= (state >> 1) ^ (state[0] ? 32'h80200003 : 32'h0);
        end
    end

    assign out = state[0];

endmodule
