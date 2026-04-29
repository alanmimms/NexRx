<Qucs Schematic 26.1.0>
<Properties>
  <View=-192,126,1209,1131,1.11741,0,0>
  <Grid=10,10,1>
  <DataSet=band4.dat>
  <DataDisplay=band4.dpl>
  <OpenDisplay=0>
  <Script=band4.m>
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
  <Pac P1 1 200 290 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 200 320 0 0 0 0>
  <GND * 1 350 320 0 0 0 0>
  <GND * 1 490 320 0 0 0 0>
  <Pac P2 1 600 290 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 600 320 0 0 0 0>
  <.SP SP1 1 250 390 0 56 0 0 "log" 1 "1.43MHz" 1 "220MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 470 400 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <C C2 1 400 210 -26 10 0 0 "68pF" 1 "" 0 "neutral" 0>
  <L L2 1 460 210 -26 -44 0 0 "1.2uH" 1 "" 0>
  <L L3 1 490 290 8 -26 0 1 "180nH" 1 "" 0>
  <L L1 1 350 290 8 -26 0 1 "180nH" 1 "" 0>
  <C C1 1 320 290 -8 46 0 1 "430pF" 1 "" 0 "neutral" 0>
  <C C3 1 460 290 -8 46 0 1 "430pF" 1 "" 0 "neutral" 0>
</Components>
<Wires>
  <200 210 200 260 "" 0 0 0 "">
  <200 210 350 210 "" 0 0 0 "">
  <350 210 350 260 "" 0 0 0 "">
  <490 210 490 260 "" 0 0 0 "">
  <350 210 370 210 "" 0 0 0 "">
  <320 260 350 260 "" 0 0 0 "">
  <320 320 350 320 "" 0 0 0 "">
  <460 260 490 260 "" 0 0 0 "">
  <460 320 490 320 "" 0 0 0 "">
  <600 210 600 260 "" 0 0 0 "">
  <490 210 600 210 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 137 1090 743 400 3 #c0c0c0 1 10 1 0 2e+06 4.2e+07 1 -80 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 580 390 12 #000000 0 "Chebyshev band-pass filter \n 14.3MHz...22MHz, pi-type, \n impedance matching 50 Ohm">
</Paintings>
