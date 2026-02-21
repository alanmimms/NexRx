# NexRx Project TODO

## FPGA RTL (SystemVerilog)
- [ ] **Phase Clock Timing:** Map all QSD phase clock outputs (`ph0`-`ph3`) to `SB_IO` primitives with dedicated flip-flops. This is critical to minimize skew between the 120MHz and 180MHz clock phases and ensure maximum image rejection.
- [ ] **Skew Constraints:** Define strict max-skew constraints in the `.pcf` or timing constraint file for all phase clock groups to preserve the integrity of the triple-QSD architecture.
- [ ] **High-Frequency Logic:** Optimize the phase generation logic for 120/180MHz operation. Given the lack of headroom for 8x oversampling, consider using the iCE40's `SB_PLL` and DDR output modes if dead-time insertion requires sub-cycle resolution.

## Hardware / Schematic
- [ ] **Attenuation Network:** Replace hierarchical attenuator sheets with flat copies using 0.1% Thin Film resistors (Susumu RG or Panasonic ERA).
- [ ] **ESD Protection:** Swap D301 to Littelfuse SP0402B-ELC-01ETG (0.15pF) to preserve RF sensitivity at 30MHz.
- [ ] **Bias Purity:** Update baseband bias decoupling to 10uF X7R at every MAX9939 input stage.
