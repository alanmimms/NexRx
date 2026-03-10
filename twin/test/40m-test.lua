-- NexRx Stimulus - 40m WAV Capture Test
-- Tests the new RFCapture functionality

print("[Stimulus] Loading 40m I/Q capture...")

stimulus.addRFCapture("40m-band", {
    path = "twin/test/40m/SDRuno_20200912_004330Z_7150kHz.wav",
    freq = 7.150e6,
    amplitude = 1.0, -- 1V peak at virtual antenna
    loop = true,
    swapIQ = true
})

print("[Stimulus] Ready. Band center: 7.150 MHz")
