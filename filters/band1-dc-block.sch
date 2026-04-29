<Qucs Schematic 26.1.1>
<Properties>
  <View=-466,296,1418,1251,1.17592,0,0>
  <Grid=10,10,1>
  <DataSet=band1-dc-block.dat>
  <DataDisplay=band1-dc-block.dpl>
  <OpenDisplay=0>
  <Script=band1-dc-block.m>
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
  <Pac P1 1 110 460 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 110 490 0 0 0 0>
  <GND * 1 330 490 0 0 0 0>
  <Pac P2 1 470 460 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0 "false" 0>
  <GND * 1 470 490 0 0 0 0>
  <.SP SP1 1 160 560 0 56 0 0 "log" 1 "180kHz" 1 "34MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 380 570 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <L L1 1 300 380 -26 -44 0 0 "3.3uH" 1 "" 0>
  <L L3 1 440 380 -26 -44 0 0 "3.3uH" 1 "" 0>
  <C C1 1 240 380 -26 10 0 0 "1.3nF" 1 "" 0 "neutral" 0>
  <C C3 1 380 380 -26 10 0 0 "1.3nF" 1 "" 0 "neutral" 0>
  <L L2 1 330 460 8 -26 0 1 "2.2uH" 1 "" 0>
  <C C2 1 300 460 -8 46 0 1 "2nF" 1 "" 0 "neutral" 0>
</Components>
<Wires>
  <110 380 110 430 "" 0 0 0 "">
  <110 380 210 380 "" 0 0 0 "">
  <330 380 330 430 "" 0 0 0 "">
  <470 380 470 430 "" 0 0 0 "">
  <330 380 350 380 "" 0 0 0 "">
  <300 430 330 430 "" 0 0 0 "">
  <300 490 330 490 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 110 1210 744 452 3 #c0c0c0 1 10 1 0 2e+06 3.4e+07 1 -82.7253 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #0000ff 1 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 490 560 12 #000000 0 "Chebyshev band-pass filter \n 1.8MHz...3.4MHz, tee-type, \n impedance matching 50 Ohm">
</Paintings>
