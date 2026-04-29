<Qucs Schematic 26.1.1>
<Properties>
  <View=0,0,2542,1131,1,0,0>
  <Grid=10,10,1>
  <DataSet=band2-dc-block.dat>
  <DataDisplay=band2-dc-block.dpl>
  <OpenDisplay=0>
  <Script=band2-dc-block.m>
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
  <Pac P1 1 700 250 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 700 280 0 0 0 0>
  <GND * 1 920 280 0 0 0 0>
  <Pac P2 1 1060 250 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 1060 280 0 0 0 0>
  <.SP SP1 1 750 350 0 56 0 0 "log" 1 "320kHz" 1 "75MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 970 360 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <L L1 1 890 170 -26 -44 0 0 "2.2uH" 1 "" 0>
  <L L3 1 1030 170 -26 -44 0 0 "2.2uH" 1 "" 0>
  <C C1 1 830 170 -26 10 0 0 "510pF" 1 "" 0 "neutral" 0>
  <C C3 1 970 170 -26 10 0 0 "510pF" 1 "" 0 "neutral" 0>
  <C C2 1 890 250 -8 46 0 1 "910pF" 1 "" 0 "neutral" 0>
  <L L2 1 920 250 8 -26 0 1 "1.2uH" 1 "" 0>
</Components>
<Wires>
  <700 170 700 220 "" 0 0 0 "">
  <700 170 800 170 "" 0 0 0 "">
  <920 170 920 220 "" 0 0 0 "">
  <1060 170 1060 220 "" 0 0 0 "">
  <920 170 940 170 "" 0 0 0 "">
  <890 220 920 220 "" 0 0 0 "">
  <890 280 920 280 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 670 1050 744 452 3 #c0c0c0 1 10 1 0 1 0 1 -1 0.2 1 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #0000ff 1 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 1080 350 12 #000000 0 "Chebyshev band-pass filter \n 3.2MHz...7.5MHz, tee-type, \n impedance matching 50 Ohm">
</Paintings>
