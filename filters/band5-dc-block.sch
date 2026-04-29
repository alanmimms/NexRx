<Qucs Schematic 26.1.1>
<Properties>
  <View=-551,146,1567,1131,1.1401,0,0>
  <Grid=10,10,1>
  <DataSet=band5-dc-block.dat>
  <DataDisplay=band5-dc-block.dpl>
  <OpenDisplay=0>
  <Script=band5-dc-block.m>
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
  <Pac P1 1 30 360 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 30 390 0 0 0 0>
  <GND * 1 250 390 0 0 0 0>
  <Pac P2 1 390 360 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 390 390 0 0 0 0>
  <.SP SP1 1 80 460 0 56 0 0 "log" 1 "2.18MHz" 1 "300MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 300 470 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <L L2 1 250 360 8 -26 0 1 "82nH" 1 "" 0>
  <C C2 1 220 360 -8 46 0 1 "430pF" 1 "" 0 "neutral" 0>
  <L L1 1 220 280 -26 -44 0 0 "1uH" 1 "" 0>
  <L L3 1 360 280 -26 -44 0 0 "1uH" 1 "" 0>
  <C C3 1 300 280 -26 10 0 0 "39pF" 1 "" 0 "neutral" 0>
  <C C1 1 160 280 -26 10 0 0 "39pF" 1 "" 0 "neutral" 0>
</Components>
<Wires>
  <30 280 30 330 "" 0 0 0 "">
  <30 280 130 280 "" 0 0 0 "">
  <250 280 250 330 "" 0 0 0 "">
  <390 280 390 330 "" 0 0 0 "">
  <250 280 270 280 "" 0 0 0 "">
  <220 330 250 330 "" 0 0 0 "">
  <220 390 250 390 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 137 1090 743 400 3 #c0c0c0 1 10 1 0 2e+06 4.2e+07 1 -80 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 410 460 12 #000000 0 "Chebyshev band-pass filter \n 21.8MHz...30MHz, tee-type, \n impedance matching 50 Ohm">
</Paintings>
