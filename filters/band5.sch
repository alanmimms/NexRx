<Qucs Schematic 26.1.0>
<Properties>
  <View=-6,91,1563,1214,1,4,0>
  <Grid=10,10,1>
  <DataSet=band5.dat>
  <DataDisplay=band5.dpl>
  <OpenDisplay=0>
  <Script=band5.m>
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
  <Pac P1 1 180 310 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 180 340 0 0 0 0>
  <GND * 1 330 340 0 0 0 0>
  <GND * 1 470 340 0 0 0 0>
  <Pac P2 1 580 310 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 580 340 0 0 0 0>
  <.SP SP1 1 230 410 0 56 0 0 "log" 1 "2.18MHz" 1 "300MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 450 420 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <L L2 1 440 230 -26 -44 0 0 "1.2uH" 1 "" 0>
  <L L3 1 470 310 8 -26 0 1 "100nH" 1 "" 0>
  <L L1 1 330 310 8 -26 0 1 "100nH" 1 "" 0>
  <C C2 1 380 230 -26 10 0 0 "33pF" 1 "" 0 "neutral" 0>
  <C C1 1 300 310 -8 46 0 1 "390pF" 1 "" 0 "neutral" 0>
  <C C3 1 440 310 -8 46 0 1 "390pF" 1 "" 0 "neutral" 0>
</Components>
<Wires>
  <180 230 180 280 "" 0 0 0 "">
  <180 230 330 230 "" 0 0 0 "">
  <330 230 330 280 "" 0 0 0 "">
  <470 230 470 280 "" 0 0 0 "">
  <330 230 350 230 "" 0 0 0 "">
  <300 280 330 280 "" 0 0 0 "">
  <300 340 330 340 "" 0 0 0 "">
  <440 280 470 280 "" 0 0 0 "">
  <440 340 470 340 "" 0 0 0 "">
  <580 230 580 280 "" 0 0 0 "">
  <470 230 580 230 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 137 1090 743 400 3 #c0c0c0 1 10 1 0 2e+06 4.2e+07 1 -80 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 560 410 12 #000000 0 "Chebyshev band-pass filter \n 21.8MHz...30MHz, pi-type, \n impedance matching 50 Ohm">
</Paintings>
