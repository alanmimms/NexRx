/**
 * nco-core.sv
 * 32-bit Numerically Controlled Oscillator with Shadow Register.
 */
module NCOCore (
    input  logic        clk,
    input  logic        resetN,
    input  logic [31:0] nextIncrement,
    input  logic        commit,
    input  logic        forceReset, /* Force phase to zero for alignment */
    output logic        pulse
);

    logic [31:0] increment;
    logic [31:0] accumulator;
    logic        accMsbPrev;

    /* Shadow Register Update */
    always_ff @(posedge clk or negedge resetN) begin
        if (!resetN) begin
            increment <= 32'h0;
        end else if (commit) begin
            increment <= nextIncrement;
        end
    end

    /* Phase Accumulator */
    always_ff @(posedge clk or negedge resetN) begin
        if (!resetN) begin
            accumulator <= 32'h0;
            accMsbPrev <= 1'b0;
        end else if (forceReset) begin
            accumulator <= 32'h0;
            accMsbPrev <= 1'b0;
        end else begin
            accumulator <= accumulator + increment;
            accMsbPrev <= accumulator[31];
        end
    end

    assign pulse = (accumulator[31] && !accMsbPrev);

endmodule
