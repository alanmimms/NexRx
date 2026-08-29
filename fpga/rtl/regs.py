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
  octMode:      Bit(0, "Generate 8 phases if true, 4 phases otherwise")
  softReset:    Bit(1, "Soft reset for internal state machines")
  reserved:     UInt(2, 30, "Reserved")

@regs.register(0x01, "GNSS PPS Latched Counter")
class PpsLatch:
  val:          UInt(0, 32, "Latched TCXO clock cycles count at PPS edge")

# We can assume these never change address.
@regs.register(0x7E, "CPLD Build Number")
class BuildNo:
  val:          UInt(0, 32, "CPLD 32-bit build number")

@regs.register(0x7F, "CPLD Hardware Signature")
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
  print(f"Generated registers at {prefix}.[h|md|sv]")
