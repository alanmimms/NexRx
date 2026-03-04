/**
 * register-bank.sv
 * SPI-accessible register map for NexRx FPGA.
 */
module RegisterBank (
    input  logic        clkSys,
    input  logic        clkTcxo,
    input  logic        resetN,

    /* SPI Interface */
    input  logic        spiSck,
    input  logic        spiMosi,
    output logic        spiMiso,
    input  logic        spiNss,

    /* Register Outputs */
    output logic [31:0] isgInc,
    output logic [31:0] qsd0Inc,
    output logic [31:0] qsd1Inc,
    output logic [31:0] qsd2Inc,
    output logic        commitPulse,

    /* Inputs from Internal Logic */
    input  logic [63:0] tcxoTimer
);

    logic [31:0] shiftReg;
    logic [5:0]  bitCnt;
    logic [7:0]  cmdLatch;
    logic        cmdDone;

    logic [6:0]  addr;
    logic        isWrite;
    logic [31:0] dataIn;
    logic [31:0] dataOut;

    logic [31:0] scratchReg;
    logic [31:0] isgIncShadow;
    logic [31:0] qsd0IncShadow;
    logic [31:0] qsd1IncShadow;
    logic [31:0] qsd2IncShadow;

    logic [31:0] timeHLatch;
    logic [31:0] tcxoTimerLow;

    assign tcxoTimerLow = tcxoTimer[31:0];

    //==================================================================
    // SPI Frontend
    //==================================================================
    always_ff @(posedge spiSck or posedge spiNss) begin
        if (spiNss) begin
            bitCnt <= 6'd0;
            cmdDone <= 1'b0;
            cmdLatch <= 8'h0;
        end else begin
            if (bitCnt < 6'd8) begin
                cmdLatch <= {cmdLatch[6:0], spiMosi};
            end else begin
                shiftReg <= {shiftReg[30:0], spiMosi};
            end

            if (bitCnt == 6'd39) begin
                cmdDone <= 1'b1;
            end else begin
                bitCnt <= bitCnt + 1'b1;
            end
        end
    end

    always_ff @(negedge spiSck or posedge spiNss) begin
        if (spiNss) begin
            spiMiso <= 1'b0;
        end else begin
            if (bitCnt >= 6'd8 && bitCnt < 6'd40) begin
                spiMiso <= dataOut[39 - bitCnt];
            end else begin
                spiMiso <= 1'b0;
            end
        end
    end

    assign addr = cmdLatch[6:0];
    assign isWrite = cmdLatch[7];
    assign dataIn = shiftReg;

    //==================================================================
    // Register Logic
    //==================================================================
    logic cmdDoneSyncQ1, cmdDoneSyncQ2;
    always_ff @(posedge clkSys) begin
        cmdDoneSyncQ1 <= cmdDone;
        cmdDoneSyncQ2 <= cmdDoneSyncQ1;
    end
    assign cmdDoneSys = cmdDoneSyncQ1 && !cmdDoneSyncQ2;

    always_ff @(posedge clkSys or negedge resetN) begin
        if (!resetN) begin
            scratchReg <= 32'h55AA;
            isgIncShadow <= 32'h0;
            qsd0IncShadow <= 32'h0;
            qsd1IncShadow <= 32'h0;
            qsd2IncShadow <= 32'h0;
            commitPulse <= 1'b0;
            timeHLatch <= 32'h0;
        end else begin
            commitPulse <= 1'b0;
            if (cmdDoneSys) begin
                if (isWrite) begin
                    case (addr)
                        7'h00: scratchReg <= dataIn;
                        7'h01: commitPulse <= 1'b1;
                        7'h10: isgIncShadow <= dataIn;
                        7'h20: qsd0IncShadow <= dataIn;
                        7'h30: qsd1IncShadow <= dataIn;
                        7'h40: qsd2IncShadow <= dataIn;
                    endcase
                end else if (addr == 7'h04) begin
                    timeHLatch <= tcxoTimer[63:32];
                end
            end
        end
    end

    always_comb begin
        case (addr)
            7'h00: dataOut = scratchReg;
            7'h04: dataOut = tcxoTimerLow;
            7'h05: dataOut = timeHLatch;
            7'h10: dataOut = isgIncShadow;
            7'h20: dataOut = qsd0IncShadow;
            7'h30: dataOut = qsd1IncShadow;
            7'h40: dataOut = qsd2IncShadow;
            default: dataOut = 32'hDEADBEEF;
        endcase
    end

    assign isgInc = isgIncShadow;
    assign qsd0Inc = qsd0IncShadow;
    assign qsd1Inc = qsd1IncShadow;
    assign qsd2Inc = qsd2IncShadow;

endmodule
