/**
 * nexrx-top.sv
 * Top-level module for NexRx FPGA.
 */
`timescale 1ns/1ps

module NexRxTop (
    input  logic clk40m,    /* 40 MHz External TCXO */
    
    /* SPI Slave Interface */
    input  logic spiSck,
    input  logic spiMosi,
    output logic spiMiso,
    input  logic spiNss,

    /* QSD Outputs */
    output logic [3:0] QSD0Clk,
    output logic [3:0] QSD1Clk,
    output logic [5:0] QSD2Clk,

    /* Internal Signal Gen */
    output logic ISGOut
);

    /* Clocking & Reset */
    logic clkSys;    /* 180 MHz Master Clock */
    logic pllLocked;
    logic resetN;

`ifdef SIMULATION
    initial clkSys = 0;
    always #2.77 clkSys = ~clkSys;
    assign pllLocked = 1'b1;
`else
    SB_PLL40_CORE #(
        .FEEDBACK_PATH("SIMPLE"),
        .DIVR(4'b0000),
        .DIVF(7'b0101101),
        .DIVQ(3'b010),
        .FILTER_RANGE(3'b010)
    ) uPll (
        .REFERENCECLK(clk40m),
        .PLLOUTCORE(clkSys),
        .LOCK(pllLocked),
        .RESETB(1'b1),
        .BYPASS(1'b0)
    );
`endif

    assign resetN = pllLocked;

    /* Internal Interconnects */
    logic [31:0] ISGInc, QSD0Inc, QSD1Inc, QSD2Inc;
    logic        commitFreq, commitPhase;
    logic [63:0] tcxoTimer;
    logic        ISGPulse, QSD0Pulse, QSD1Pulse, QSD2Pulse;
    logic        noiseBit;
    logic        ISGTone;

    //==================================================================
    // Register Bank & SPI
    //==================================================================
    RegisterBank uRegBank (
        .clkSys(clkSys),
        .clkTcxo(clk40m),
        .resetN(resetN),
        .spiSck(spiSck),
        .spiMosi(spiMosi),
        .spiMiso(spiMiso),
        .spiNss(spiNss),
        .ISGInc(ISGInc),
        .QSD0Inc(QSD0Inc),
        .QSD1Inc(QSD1Inc),
        .QSD2Inc(QSD2Inc),
        .commitFreq(commitFreq),
        .commitPhase(commitPhase),
        .tcxoTimer(tcxoTimer)
    );

    //==================================================================
    // Noise Generator
    //==================================================================
    NoiseGen uNoise (.clk(clkSys), .resetN(resetN), .out(noiseBit));

    //==================================================================
    // Precision Monotonic Timer
    //==================================================================
    Timer64Bit uTimer (
        .clk40m(clk40m),
        .resetN(resetN),
        .latchTrigger(1'b0),
        .countLow(tcxoTimer[31:0]),
        .countHigh(tcxoTimer[63:32])
    );

    //==================================================================
    // NCO Cores
    //==================================================================
    NCOCore uNCOIsg  (.clk(clkSys), .resetN(resetN), .nextIncrement(ISGInc),  .commit(commitFreq), .forceReset(commitPhase), .pulse(ISGPulse));
    NCOCore uNCOQSD0 (.clk(clkSys), .resetN(resetN), .nextIncrement(QSD0Inc), .commit(commitFreq), .forceReset(commitPhase), .pulse(QSD0Pulse));
    NCOCore uNCOQSD1 (.clk(clkSys), .resetN(resetN), .nextIncrement(QSD1Inc), .commit(commitFreq), .forceReset(commitPhase), .pulse(QSD1Pulse));
    NCOCore uNCOQSD2 (.clk(clkSys), .resetN(resetN), .nextIncrement(QSD2Inc), .commit(commitFreq), .forceReset(commitPhase), .pulse(QSD2Pulse));

    //==================================================================
    // Phase Generators (Walking Rings)
    //==================================================================
    PhaseGen4 uGenQSD0 (.clk(clkSys), .resetN(resetN), .pulse(QSD0Pulse), .phases(QSD0Clk));
    PhaseGen4 uGenQSD1 (.clk(clkSys), .resetN(resetN), .pulse(QSD1Pulse), .phases(QSD1Clk));
    PhaseGen6 uGenQSD2 (.clk(clkSys), .resetN(resetN), .pulse(QSD2Pulse), .phases(QSD2Clk));

    //==================================================================
    // ISG Logic & Mode Mux
    //==================================================================
    always_ff @(posedge clkSys or negedge resetN) begin
        if (!resetN)
            ISGTone <= 1'b0;
        else if (ISGPulse)
            ISGTone <= ~ISGTone;
    end

    always_comb begin
        if (ISGInc == 32'd0)
            ISGOut = 1'b0;
        else if (ISGInc == 32'd1)
            ISGOut = noiseBit;
        else
            ISGOut = ISGTone;
    end

endmodule
