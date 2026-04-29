<Qucs Schematic 26.1.1>
<Properties>
  <View=-261,246,1941,1271,1.09668,0,0>
  <Grid=10,10,1>
  <DataSet=am-reject-hpf-dc-block.dat>
  <DataDisplay=am-reject-hpf-dc-block.dpl>
  <OpenDisplay=0>
  <Script=am-reject-hpf-dc-block.m>
  <RunScript=0>
  <showFrame=0>
  <FrameText0=Title>
  <FrameText1=Drawn By:>
  <FrameText2=Date:>
  <FrameText3=Revision:>
</Properties>
<Symbol>
</Symbol>
<Components>
  <Pac P1 1 270 400 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 270 430 0 0 0 0>
  <GND * 1 450 430 0 0 0 0>
  <GND * 1 590 430 0 0 0 0>
  <GND * 1 730 430 0 0 0 0>
  <Pac P2 1 870 400 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 870 430 0 0 0 0>
  <.SP SP1 1 280 500 0 56 0 0 "log" 1 "175kHz" 1 "17.5MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 500 510 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <C C1 1 380 320 -27 10 0 0 "1.6nF" 1 "" 0 "neutral" 0>
  <C C4 1 800 320 -27 10 0 0 "1.6nF" 1 "" 0 "neutral" 0>
  <L L1 1 450 400 17 -26 0 1 "3.3uH" 1 "" 0>
  <L L3 1 730 400 17 -26 0 1 "3.3uH" 1 "" 0>
  <L L2 1 590 400 17 -26 0 1 "3.3uH" 1 "" 0>
  <C C2 1 520 320 -27 10 0 0 "910pF" 1 "" 0 "neutral" 0>
  <C C3 1 660 320 -27 10 0 0 "910pF" 1 "" 0 "neutral" 0>
</Components>
<Wires>
  <270 320 270 370 "" 0 0 0 "">
  <270 320 350 320 "" 0 0 0 "">
  <450 320 450 370 "" 0 0 0 "">
  <590 320 590 370 "" 0 0 0 "">
  <730 320 730 370 "" 0 0 0 "">
  <870 320 870 370 "" 0 0 0 "">
  <410 320 450 320 "" 0 0 0 "">
  <450 320 490 320 "" 0 0 0 "">
  <550 320 590 320 "" 0 0 0 "">
  <590 320 630 320 "" 0 0 0 "">
  <690 320 730 320 "" 0 0 0 "">
  <730 320 770 320 "" 0 0 0 "">
  <830 320 870 320 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 327 1230 1147 614 3 #c0c0c0 1 10 1 0 2e+06 4.2e+07 1 -80 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 610 500 12 #000000 0 "Chebyshev high-pass filter \n 1.75MHz cutoff, tee-type, \n impedance matching 50 Ohm">
</Paintings>
