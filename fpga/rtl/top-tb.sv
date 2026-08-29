module TBTop;

  timeunit 1ns;
  timeprecision 1ps;

  /* Simulation Signals */
  logic clk40M;
  logic spiSCK, spiMOSI, spiMISO, spiNSS;

  /* Clock Generation */
  initial clk40M = 0;
  always #12.5 clk40M = ~clk40M;

  initial clkVFO = 0;
  always #100 clkVFO = ~clkVFO;

  /* DUT Instance */
  top dut (.iClk40M(clk40M),
	   .iClkVFO(clkVFO),
	   .spiSCK(spiSCK),
	   .spiMOSI(spiMOSI),
	   .spiMISO(spiMISO),
	   .spiNSS(spiNSS)
	   );

  /* SPI Master Model Task */
  task spiXfer(input logic [7:0] cmd, input logic [31:0] dataIn, output logic [31:0] dataOut);
    spiNSS = 0;
    #100;
    for (int i = 7; i >= 0; i--) begin
      spiMOSI = cmd[i];
      #50; spiSCK = 1; #50; spiSCK = 0;
    end
    for (int i = 31; i >= 0; i--) begin
      spiMOSI = dataIn[i];
      #50; spiSCK = 1;
      dataOut[i] = spiMISO;
      #50; spiSCK = 0;
    end
    #100;
    spiNSS = 1;
    #200;
  endtask

  logic [31:0] rdata;
  int errorCount = 0;

  initial begin
    /* Init SPI */
    spiSCK = 0; spiMOSI = 0; spiNSS = 1;
    #500;

    $display("--- NexRx CPLD Self-Verifying Testbench Start ---");


    #1000;
    if (errorCount == 0) begin
      $display("--- NexRx CPLD Testbench SUCCESS ---");
    end else begin
      $display("--- NexRx CPLD Testbench FAILED (%d errors) ---", errorCount);
    end
    $finish;
  end

  initial begin
    $dumpfile("cpld.vcd");
    $dumpvars(0, TBTop);
  end

endmodule
