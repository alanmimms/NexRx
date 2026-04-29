<Qucs Schematic 26.1.0>
<Properties>
  <View=-6,31,1563,1274,1,4,120>
  <Grid=10,10,1>
  <DataSet=am-reject-hpf.dat>
  <DataDisplay=am-reject-hpf.dpl>
  <OpenDisplay=0>
  <Script=am-reject-hpf.m>
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
  <Pac P1 1 220 380 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 220 410 0 0 0 0>
  <GND * 1 330 410 0 0 0 0>
  <GND * 1 470 410 0 0 0 0>
  <GND * 1 610 410 0 0 0 0>
  <Pac P2 1 720 380 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 720 410 0 0 0 0>
  <.SP SP1 1 230 480 0 56 0 0 "log" 1 "175kHz" 1 "17.5MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 450 490 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <C C1 1 400 300 -27 10 0 0 "1.5nF" 1 "" 0 "neutral" 0>
  <C C2 1 540 300 -27 10 0 0 "1.5nF" 1 "" 0 "neutral" 0>
  <L L1 1 330 380 17 -26 0 1 "4.7uH" 1 "" 0>
  <L L3 1 610 380 17 -26 0 1 "4.7uH" 1 "" 0>
  <L L2 1 470 380 17 -26 0 1 "2.7uH" 1 "" 0>
</Components>
<Wires>
  <220 300 220 350 "" 0 0 0 "">
  <220 300 330 300 "" 0 0 0 "">
  <330 300 330 350 "" 0 0 0 "">
  <470 300 470 350 "" 0 0 0 "">
  <610 300 610 350 "" 0 0 0 "">
  <330 300 370 300 "" 0 0 0 "">
  <430 300 470 300 "" 0 0 0 "">
  <470 300 510 300 "" 0 0 0 "">
  <570 300 610 300 "" 0 0 0 "">
  <720 300 720 350 "" 0 0 0 "">
  <610 300 720 300 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 327 1230 1147 614 3 #c0c0c0 1 10 1 0 2e+06 4.2e+07 1 -80 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 560 480 12 #000000 0 "Chebyshev high-pass filter \n 1.75MHz cutoff, pi-type, \n impedance matching 50 Ohm">
</Paintings>
