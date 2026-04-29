<Qucs Schematic 26.1.0>
<Properties>
  <View=-6,91,1563,1214,1,4,0>
  <Grid=10,10,1>
  <DataSet=band1.dat>
  <DataDisplay=band1.dpl>
  <OpenDisplay=0>
  <Script=band1.m>
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
  <Pac P1 1 210 390 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 210 420 0 0 0 0>
  <GND * 1 360 420 0 0 0 0>
  <GND * 1 500 420 0 0 0 0>
  <Pac P2 1 610 390 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 610 420 0 0 0 0>
  <.SP SP1 1 260 490 0 56 0 0 "log" 1 "180kHz" 1 "34MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 480 500 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <L L1 1 360 390 8 -26 0 1 "2.2uH" 1 "" 0>
  <L L3 1 500 390 8 -26 0 1 "2.2uH" 1 "" 0>
  <L L2 1 470 310 -26 -44 0 0 "5.6uH" 1 "" 0>
  <C C1 1 330 390 -8 46 0 1 "2nF" 1 "" 0 "neutral" 0>
  <C C3 1 470 390 -8 46 0 1 "2nF" 1 "" 0 "neutral" 0>
  <C C2 1 410 310 -26 10 0 0 "750pF" 1 "" 0 "neutral" 0>
</Components>
<Wires>
  <210 310 210 360 "" 0 0 0 "">
  <210 310 360 310 "" 0 0 0 "">
  <360 310 360 360 "" 0 0 0 "">
  <500 310 500 360 "" 0 0 0 "">
  <360 310 380 310 "" 0 0 0 "">
  <330 360 360 360 "" 0 0 0 "">
  <330 420 360 420 "" 0 0 0 "">
  <470 360 500 360 "" 0 0 0 "">
  <470 420 500 420 "" 0 0 0 "">
  <610 310 610 360 "" 0 0 0 "">
  <500 310 610 310 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 137 1090 743 400 3 #c0c0c0 1 10 1 0 2e+06 4.2e+07 1 -80 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 590 490 12 #000000 0 "Chebyshev band-pass filter \n 1.8MHz...3.4MHz, pi-type, \n impedance matching 50 Ohm">
</Paintings>
