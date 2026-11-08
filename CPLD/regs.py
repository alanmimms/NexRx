#!/usr/bin/env python3
import sys
import os

# Python 3.7 or later is required for order-preserving dicts.
if sys.version_info < (3, 7):
  sys.exit(f"FATAL ERROR: This requires Python 3.7 or higher.")

from regTool import RegisterSet, UInt, Bit, Enum, Int


regs = RegisterSet("CPLD")


@regs.register(0x00, "Main control and status")
class Control:
  octMode:      Bit(1, "Generate 8 phases if true, 4 phases otherwise")
  counterThaw:  Bit(0, "Unthaw to start on next 1pps & clock")
  counterState: Enum("waiting", [
                        ("waiting",   "Waiting for 1PPS high to start counting"),
                        ("counting1", "Counting TCXO pulses while 1PPS high"),
                        ("counting2", "Counting TCXO pulses while 1PPS low"),
                        ("frozen",    "Frozen until control.counterThaw"),
                      ],
                     doc="State of TCXO counter")

# Read all 32 bits at one go.
@regs.register(0x04, "GNSS TCXO Latched Counter")
class Counter:
  val:          UInt(0, 32, "Latched TCXO clock cycles count at second PPS edge")


# We can assume this never changes address.
@regs.register(0x7C, "CPLD Hardware Signature")
class Sig:
  val:          Enum(0x4E785278, 32, [("", 0x4E785278)], "Fixed value ASCII 'NxRx'")


if __name__ == "__main__":

  if len(sys.argv) > 1:
    genDir = sys.argv[1]
  else:
    genDir = "gen" # Fallback if no param is passed
    print("No genDir parameter provided, using 'gen/'.")
  
  os.makedirs(genDir, exist_ok=True)
  prefix = os.path.join(genDir, "regs")
  regs.writeFiles(prefix)
  print(f"Generated registers into {prefix}.{{h,md,sv}}")
