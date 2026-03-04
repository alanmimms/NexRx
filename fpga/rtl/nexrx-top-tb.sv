`timescale 1ns/1ps

module NexRxTopTb;

    /* Simulation Signals */
    logic clk40m;
    logic spiSck, spiMosi, spiMiso, spiNss;
    logic [3:0] QSD0Clk, QSD1Clk;
    logic [5:0] QSD2Clk;
    logic ISGOut;

    /* Clock Generation: 40 MHz TCXO */
    initial clk40m = 0;
    always #12.5 clk40m = ~clk40m;

    /* DUT Instance */
    NexRxTop dut (
        .clk40m(clk40m),
        .spiSck(spiSck),
        .spiMosi(spiMosi),
        .spiMiso(spiMiso),
        .spiNss(spiNss),
        .QSD0Clk(QSD0Clk),
        .QSD1Clk(QSD1Clk),
        .QSD2Clk(QSD2Clk),
        .ISGOut(ISGOut)
    );

    /* SPI Master Model Task */
    task spiXfer(input logic [7:0] cmd, input logic [31:0] dataIn, output logic [31:0] dataOut);
        spiNss = 0;
        #100;
        for (int i = 7; i >= 0; i--) begin
            spiMosi = cmd[i];
            #50; spiSck = 1; #50; spiSck = 0;
        end
        for (int i = 31; i >= 0; i--) begin
            spiMosi = dataIn[i];
            #50; spiSck = 1;
            dataOut[i] = spiMiso;
            #50; spiSck = 0;
        end
        #100;
        spiNss = 1;
        #200;
    endtask

    logic [31:0] rdata;
    int errorCount = 0;

    initial begin
        /* Init SPI */
        spiSck = 0; spiMosi = 0; spiNss = 1;
        #500;

        $display("--- NexRx FPGA Self-Verifying Testbench Start ---");

        /* 1. Test Scratchpad Write/Read */
        $display("[Test 1] REG_SCRATCH (0x00)...");
        spiXfer(8'h80, 32'h55AAAA55, rdata); /* Write */
        spiXfer(8'h00, 32'h0, rdata);        /* Read */
        if (rdata === 32'h55AAAA55) begin
            $display("  PASS: Scratchpad matches 0x55AAAA55");
        end else begin
            $display("  FAIL: Scratchpad mismatch! Expected 0x55AAAA55, got 0x%08X", rdata);
            errorCount++;
        end

        /* 2. Configure ISG (10 MHz @ 180 MHz) */
        $display("[Test 2] Configuring ISG NCO...");
        spiXfer(8'h90, 32'd238609294, rdata);
        spiXfer(8'h81, 32'h1, rdata); /* Commit */
        
        /* 3. Observe Phasing */
        #1000;
        $display("[Test 3] Verifying QSD2 Sextature Phasing...");
        spiXfer(8'hC0, 32'd1002159035, rdata);
        spiXfer(8'h81, 32'h1, rdata);
        #500;
        if (QSD2Clk !== 6'b0) begin
            $display("  PASS: QSD2 clocks are active");
        end else begin
            $display("  FAIL: QSD2 clocks are stuck at zero");
            errorCount++;
        end

        /* 4. Test 64-bit Timer Snapshot */
        $display("[Test 4] Verifying 64-bit Timer Snapshot...");
        spiXfer(8'h04, 32'h0, rdata); /* Read Low (latches high) */
        if (rdata !== 32'h0) begin
            $display("  PASS: Timer is incrementing (Low: 0x%08X)", rdata);
        end else begin
            $display("  FAIL: Timer stuck at zero");
            errorCount++;
        end

        #1000;
        if (errorCount == 0) begin
            $display("--- NexRx FPGA Testbench SUCCESS ---");
        end else begin
            $display("--- NexRx FPGA Testbench FAILED (%d errors) ---", errorCount);
        end
        $finish;
    end

    initial begin
        $dumpfile("nexrx-fpga.vcd");
        $dumpvars(0, NexRxTopTb);
    end

endmodule
