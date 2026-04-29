<Qucs Schematic 26.1.0>
<Properties>
  <View=-108,246,1125,1131,1.26893,0,0>
  <Grid=10,10,1>
  <DataSet=band2.dat>
  <DataDisplay=band2.dpl>
  <OpenDisplay=0>
  <Script=band2.m>
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
  <Pac P1 1 210 410 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 210 440 0 0 0 0>
  <GND * 1 360 440 0 0 0 0>
  <GND * 1 500 440 0 0 0 0>
  <Pac P2 1 610 410 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0 "true" 0>
  <GND * 1 610 440 0 0 0 0>
  <.SP SP1 1 260 510 0 56 0 0 "log" 1 "320kHz" 1 "75MHz" 1 "201" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Eqn1 1 480 520 -28 15 0 0 "dBS21=dB(S[2,1])" 1 "dBS11=dB(S[1,1])" 1 "yes" 0>
  <L L2 1 470 330 -26 -44 0 0 "2.2uH" 1 "" 0>
  <L L3 1 500 410 8 -26 0 1 "1.5uH" 1 "" 0>
  <L L1 1 360 410 8 -26 0 1 "1.5uH" 1 "" 0>
  <C C1 1 330 410 -8 46 0 1 "750pF" 1 "" 0 "neutral" 0>
  <C C3 1 470 410 -8 46 0 1 "750pF" 1 "" 0 "neutral" 0>
  <C C2 1 410 330 -26 10 0 0 "500pF" 1 "" 0 "neutral" 0>
</Components>
<Wires>
  <210 330 210 380 "" 0 0 0 "">
  <210 330 360 330 "" 0 0 0 "">
  <360 330 360 380 "" 0 0 0 "">
  <500 330 500 380 "" 0 0 0 "">
  <360 330 380 330 "" 0 0 0 "">
  <330 380 360 380 "" 0 0 0 "">
  <330 440 360 440 "" 0 0 0 "">
  <470 380 500 380 "" 0 0 0 "">
  <470 440 500 440 "" 0 0 0 "">
  <610 330 610 380 "" 0 0 0 "">
  <500 330 610 330 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 137 1090 743 400 3 #c0c0c0 1 10 1 0 2e+06 4.2e+07 1 -80 10 10 1 -1 0.2 1 315 0 225 1 0 0 "" "" "">
	<"dBS21" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
  <Text 590 510 12 #000000 0 "Chebyshev band-pass filter \n 3.2MHz...7.5MHz, pi-type, \n impedance matching 50 Ohm">
</Paintings>
