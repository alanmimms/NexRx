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
    output logic [31:0] ISGInc,
    output logic [31:0] QSD0Inc,
    output logic [31:0] QSD1Inc,
    output logic [31:0] QSD2Inc,
    output logic        commitFreq,
    output logic        commitPhase,

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
    logic [31:0] ISGIncShadow;
    logic [31:0] QSD0IncShadow;
    logic [31:0] QSD1IncShadow;
    logic [31:0] QSD2IncShadow;

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
            ISGIncShadow <= 32'h0;
            QSD0IncShadow <= 32'h0;
            QSD1IncShadow <= 32'h0;
            QSD2IncShadow <= 32'h0;
            commitFreq <= 1'b0;
            commitPhase <= 1'b0;
            timeHLatch <= 32'h0;
        end else begin
            commitFreq <= 1'b0;
            commitPhase <= 1'b0;
            if (cmdDoneSys) begin
                if (isWrite) begin
                    case (addr)
                        7'h00: scratchReg <= dataIn;
                        7'h01: begin
                            commitFreq <= dataIn[0];
                            commitPhase <= dataIn[1];
                        end
                        7'h10: ISGIncShadow <= dataIn;
                        7'h20: QSD0IncShadow <= dataIn;
                        7'h30: QSD1IncShadow <= dataIn;
                        7'h40: QSD2IncShadow <= dataIn;
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
            7'h02: dataOut = 32'h4E585258; /* "NXRX" */
            7'h03: dataOut = 32'h00010000; /* v1.0.0 */
            7'h04: dataOut = tcxoTimerLow;
            7'h05: dataOut = timeHLatch;
            7'h10: dataOut = ISGIncShadow;
            7'h20: dataOut = QSD0IncShadow;
            7'h30: dataOut = QSD1IncShadow;
            7'h40: dataOut = QSD2IncShadow;
            default: dataOut = 32'hDEADBEEF;
        endcase
    end

    assign ISGInc = ISGIncShadow;
    assign QSD0Inc = QSD0IncShadow;
    assign QSD1Inc = QSD1IncShadow;
    assign QSD2Inc = QSD2IncShadow;

endmodule
