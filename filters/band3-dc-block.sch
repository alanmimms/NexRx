<Qucs Schematic 26.1.1>
<Properties>
  <View=-432,256,1450,1131,1.28343,0,0>
  <Grid=10,10,1>
  <DataSet=band3-dc-block.dat>
  <DataDisplay=band3-dc-block.dpl>
  <OpenDisplay=0>
  <Script=band3-dc-block.m>
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
  <Pac P1 1 170 400 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 170 430 0 0 0 0>
  <GND * 1 390 430 0 0 0 0>
  <Pac P2 1 530 400 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 530 430 0 0 0 0>
  <.SP SP1 1 220 500 0 56 0 0 "log" 1 "730kHz" 1 "145MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 440 510 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <L L1 1 360 320 -26 -44 0 0 "1.2uH" 1 "" 0>
  <C C1 1 300 320 -26 10 0 0 "200pF" 1 "" 0 "neutral" 0>
  <C C3 1 440 320 -26 10 0 0 "200pF" 1 "" 0 "neutral" 0>
  <L L3 1 500 320 -26 -44 0 0 "1.2uH" 1 "" 0>
  <C C2 1 360 400 -8 46 0 1 "510pF" 1 "" 0 "neutral" 0>
  <L L2 1 390 400 8 -26 0 1 "470nH" 1 "" 0>
</Components>
<Wires>
  <170 320 170 370 "" 0 0 0 "">
  <170 320 270 320 "" 0 0 0 "">
  <390 320 390 370 "" 0 0 0 "">
  <530 320 530 370 "" 0 0 0 "">
  <390 320 410 320 "" 0 0 0 "">
  <360 370 390 370 "" 0 0 0 "">
  <360 430 390 430 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 137 1090 743 400 3 #c0c0c0 1 10 1 0 2e+06 4.2e+07 1 -80 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 550 500 12 #000000 0 "Chebyshev band-pass filter \n 7.3MHz...14.5MHz, tee-type, \n impedance matching 50 Ohm">
</Paintings>
