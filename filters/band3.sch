<Qucs Schematic 26.1.0>
<Properties>
  <View=-101,256,1118,1131,1.28343,0,0>
  <Grid=10,10,1>
  <DataSet=band3.dat>
  <DataDisplay=band3.dpl>
  <OpenDisplay=0>
  <Script=band3.m>
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
  <Pac P1 1 200 420 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 200 450 0 0 0 0>
  <GND * 1 350 450 0 0 0 0>
  <GND * 1 490 450 0 0 0 0>
  <Pac P2 1 600 420 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 600 450 0 0 0 0>
  <.SP SP1 1 250 520 0 56 0 0 "log" 1 "730kHz" 1 "145MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 470 530 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <L L2 1 460 340 -26 -44 0 0 "1.2uH" 1 "" 0>
  <C C2 1 400 340 -26 10 0 0 "180pF" 1 "" 0 "neutral" 0>
  <C C1 1 320 420 -8 46 0 1 "470pF" 1 "" 0 "neutral" 0>
  <C C3 1 460 420 -8 46 0 1 "470pF" 1 "" 0 "neutral" 0>
  <L L1 1 350 420 8 -26 0 1 "560nH" 1 "" 0>
  <L L3 1 490 420 8 -26 0 1 "560nH" 1 "" 0>
</Components>
<Wires>
  <200 340 200 390 "" 0 0 0 "">
  <200 340 350 340 "" 0 0 0 "">
  <350 340 350 390 "" 0 0 0 "">
  <490 340 490 390 "" 0 0 0 "">
  <350 340 370 340 "" 0 0 0 "">
  <320 390 350 390 "" 0 0 0 "">
  <320 450 350 450 "" 0 0 0 "">
  <460 390 490 390 "" 0 0 0 "">
  <460 450 490 450 "" 0 0 0 "">
  <600 340 600 390 "" 0 0 0 "">
  <490 340 600 340 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 137 1090 743 400 3 #c0c0c0 1 10 1 0 2e+06 4.2e+07 1 -80 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 580 520 12 #000000 0 "Chebyshev band-pass filter \n 7.3MHz...14.5MHz, pi-type, \n impedance matching 50 Ohm">
</Paintings>
