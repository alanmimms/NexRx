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
    output logic [3:0] qsd0Clk,
    output logic [3:0] qsd1Clk,
    output logic [5:0] qsd2Clk,

    /* Internal Signal Gen */
    output logic isgOut
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
    logic [31:0] isgInc, qsd0Inc, qsd1Inc, qsd2Inc;
    logic        commitPulse;
    logic [63:0] tcxoTimer;
    logic        isgPulse, qsd0Pulse, qsd1Pulse, qsd2Pulse;

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
        .isgInc(isgInc),
        .qsd0Inc(qsd0Inc),
        .qsd1Inc(qsd1Inc),
        .qsd2Inc(qsd2Inc),
        .commitPulse(commitPulse),
        .tcxoTimer(tcxoTimer)
    );

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
    NcoCore uNcoIsg  (.clk(clkSys), .resetN(resetN), .nextIncrement(isgInc),  .commit(commitPulse), .pulse(isgPulse));
    NcoCore uNcoQsd0 (.clk(clkSys), .resetN(resetN), .nextIncrement(qsd0Inc), .commit(commitPulse), .pulse(qsd0Pulse));
    NcoCore uNcoQsd1 (.clk(clkSys), .resetN(resetN), .nextIncrement(qsd1Inc), .commit(commitPulse), .pulse(qsd1Pulse));
    NcoCore uNcoQsd2 (.clk(clkSys), .resetN(resetN), .nextIncrement(qsd2Inc), .commit(commitPulse), .pulse(qsd2Pulse));

    //==================================================================
    // Phase Generators (Walking Rings)
    //==================================================================
    PhaseGen4 uGenQsd0 (.clk(clkSys), .resetN(resetN), .pulse(qsd0Pulse), .phases(qsd0Clk));
    PhaseGen4 uGenQsd1 (.clk(clkSys), .resetN(resetN), .pulse(qsd1Pulse), .phases(qsd1Clk));
    PhaseGen6 uGenQsd2 (.clk(clkSys), .resetN(resetN), .pulse(qsd2Pulse), .phases(qsd2Clk));

    /* Simple ISG output toggle */
    always_ff @(posedge clkSys or negedge resetN) begin
        if (!resetN)
            isgOut <= 1'b0;
        else if (isgPulse)
            isgOut <= ~isgOut;
    end

endmodule
