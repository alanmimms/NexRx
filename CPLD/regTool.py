#!/usr/bin/env python3
import sys
import os

def capFirst(s):
  return f"{s[0].upper()}{s[1:]}" if s else ""

def lowFirst(s):
  return f"{s[0].lower()}{s[1:]}" if s else ""

class Field:

  def __init__(self, bits, default=0, doc="", signed=False):
    self.bits = bits
    self.default = default
    self.doc = doc
    self.signed = signed
    self.offset = 0
    self.name = ""

class UInt(Field):

  def __init__(self, default, bits, doc=""):
    super().__init__(bits, default, doc, signed=False)

class Int(Field):

  def __init__(self, default, bits, doc=""):
    super().__init__(bits, default, doc, signed=True)

class Bit(Field):

  def __init__(self, default, doc=""):
    super().__init__(1, default, doc, signed=False)

class Enum(Field):

  def __init__(self, default, valuesOrBits, values=None, doc="", bits=None):
    # Backwards compatibility for Enum(default, bits, values, doc)
    if isinstance(valuesOrBits, int) and values is not None:
      bits = valuesOrBits
      rawValues = values
    else:
      rawValues = valuesOrBits
      if isinstance(values, str) and not doc:
        doc = values

    self.values = []
    lastVal = -1

    if isinstance(rawValues, str):
      items = [x.strip() for x in rawValues.replace(",", " ").split() if x.strip()]
    elif isinstance(rawValues, dict):
      items = [(k, v) for k, v in rawValues.items()]
    else:
      items = list(rawValues)

    for v in items:
      vDoc = ""
      if isinstance(v, str):
        name = v
        val = lastVal + 1
      elif isinstance(v, tuple):
        if len(v) == 2:
          name, second = v
          if isinstance(second, int):
            val = second
          elif isinstance(second, tuple):
            val, vDoc = second
          else:
            val = lastVal + 1
            vDoc = str(second)
        elif len(v) == 3:
          name, val, vDoc = v
        else:
          raise ValueError(f"Invalid enum tuple format: {v}")
      else:
        raise ValueError(f"Invalid enum value format: {v}")

      self.values.append((name, val, vDoc))
      lastVal = val

    maxVal = max((val for _, val, _ in self.values), default=0)
    calcBits = max(1, maxVal.bit_length())

    if bits is None:
      bits = calcBits
    elif bits < calcBits:
      raise ValueError(f"Enum requires at least {calcBits} bits for max value {maxVal}, but bits={bits} was specified.")

    resolvedDefault = default
    if isinstance(default, str):
      for vName, vVal, _ in self.values:
        if vName == default:
          resolvedDefault = vVal
          break
      else:
        raise ValueError(f"Default enum symbolic value '{default}' not found in {[v[0] for v in self.values]}")

    super().__init__(bits, resolvedDefault, doc, signed=False)

  def getDefaultName(self):
    for vName, vVal, _ in self.values:
      if vVal == self.default:
        return vName
    return None

class Register:

  def __init__(self, name, addr, fieldsDict, doc=""):
    self.name = name
    self.addr = addr
    self.doc = doc
    self.fields = []
    self.fieldsByName = {}

    currentOffset = 0

    for fName, fObj in fieldsDict.items():
      fObj.name = fName
      fObj.offset = currentOffset
      self.fields.append(fObj)
      self.fieldsByName[fName] = fObj
      currentOffset += fObj.bits

    self.totalBits = max(8, (currentOffset + 7) & ~7)

    # Auto-pad to 8-bit boundary if needed
    if currentOffset < self.totalBits:
      pad = UInt(0, self.totalBits - currentOffset)
      pad.name = "reserved"
      pad.offset = currentOffset
      self.fields.append(pad)

  def __getitem__(self, key):
    if key not in self.fieldsByName:
      raise KeyError(f"Field '{key}' not found in register '{self.name}'")
    return self.fieldsByName[key]

  def __getattr__(self, name):
    """Allows dot-notation access to fields (e.g., my_reg.txEnable)"""
    if name in self.fieldsByName:
      return self.fieldsByName[name]
    raise AttributeError(f"'{type(self).__name__}' object has no field '{name}'")

class RegisterSet:

  def __init__(self, namespace):
    self.namespace = namespace
    self.registers = []
    self.regsByName = {}

  def register(self, addr, doc=""):
    def wrapper(cls):
      annotations = getattr(cls, "__annotations__", {})
      reg = Register(cls.__name__, addr, annotations, doc)
      self.registers.append(reg)
      self.regsByName[cls.__name__] = reg
      return cls
    return wrapper

  def __getitem__(self, key):
    if key not in self.regsByName:
      raise KeyError(f"Register '{key}' not found in RegisterSet '{self.namespace}'")
    return self.regsByName[key]

  def __getattr__(self, name):
    """Allows dot-notation access to registers (e.g., regs.Control)"""
    if name in self.regsByName:
      return self.regsByName[name]
    raise AttributeError(f"'{type(self).__name__}' object has no register '{name}'")

  def _svEnumValName(self, reg, f, vName, enumFieldCounts):
    prefix = f"{lowFirst(reg.name)}{capFirst(f.name)}" if enumFieldCounts.get(f.name, 0) > 1 else f.name
    if vName:
      return f"{prefix}{capFirst(vName)}"
    return f"{lowFirst(reg.name)}{capFirst(f.name)}"

  ################################################################
  def emitSV(self):
    lines = [
      f"// Generated SystemVerilog Register Package for {self.namespace}",
      f"package {self.namespace}Regs;",
      f"`ifdef VERILATOR",
      f"  timeunit 1ns;",
      f"  timeprecision 1ps;",
      f"`endif",
      ""
    ]

    enumFieldCounts = {}
    for reg in self.registers:
      for f in reg.fields:
        if isinstance(f, Enum):
          enumFieldCounts[f.name] = enumFieldCounts.get(f.name, 0) + 1

    # Emit widths
    for reg in self.registers:
      lines.append(f"  localparam w{self.namespace}{reg.name} = {reg.totalBits};")
    lines.append(f"  localparam w{self.namespace}Addr = 7;")
    lines.append("")

    # Emit Enums
    for reg in self.registers:
      for f in reg.fields:
        if isinstance(f, Enum):
          enumName = f"t{reg.name}{capFirst(f.name)}"
          bitsStr = f"[{f.bits-1}:0] " if f.bits > 1 else ""
          lines.append(f"  typedef enum logic {bitsStr}{{")

          for idx, (vName, vVal, vDoc) in enumerate(f.values):
            symName = self._svEnumValName(reg, f, vName, enumFieldCounts)
            comma = "," if idx < len(f.values) - 1 else ""
            docComment = f" // {vDoc}" if vDoc else ""
            lines.append(f"    {symName} = {f.bits}'h{vVal:X}{comma}{docComment}")

          lines.append(f"  }} {enumName};")
          lines.append("")

    # Emit Packed Structs
    for reg in self.registers:
      lines.append(f"  typedef struct packed {{")

      # Iterating in reversed order handles SV's MSB-to-LSB struct packing
      for f in reversed(reg.fields):
        signedStr = "signed " if f.signed else ""
        bitsStr = f"[{f.bits-1}:0]" if f.bits > 1 else "     "

        if isinstance(f, Enum):
          typeStr = f"t{reg.name}{capFirst(f.name)}"
        else:
          typeStr = f"logic {signedStr}{bitsStr}"

        fName = f"reserved{f.offset}" if f.name == "reserved" else f.name
        docStr = f"\t// {f.doc}" if f.doc else ""
        lines.append(f"    {typeStr} {fName};{docStr}")

      lines.append(f"  }} t{self.namespace}{reg.name};")

      initVals = []
      for f in reversed(reg.fields):
        if isinstance(f, Enum):
          defName = f.getDefaultName()
          if defName is not None:
            initVals.append(self._svEnumValName(reg, f, defName, enumFieldCounts))
          else:
            initVals.append(f"t{reg.name}{capFirst(f.name)}'({f.bits}'h{f.default:X})")
        else:
          initVals.append(f"{f.bits}'h{f.default:X}")

      lines.append(f"  localparam t{self.namespace}{reg.name} init{self.namespace}{reg.name} = {{{', '.join(initVals)}}};")
      lines.append("")

    # Emit Address Map
    lines.append(f"  typedef enum logic [6:0] {{")
    for idx, reg in enumerate(self.registers):
      docStr = f"\t// {reg.doc}" if reg.doc else ""
      comma = "," if idx < len(self.registers) - 1 else ""
      lines.append(f"    a{self.namespace}{reg.name} = 7'h{reg.addr:02X}{comma}{docStr}")

    lines.append(f"  }} t{self.namespace}Addr;")
    lines.append("")
    lines.append("endpackage")
    return "\n".join(lines)

  ################################################################
  def emitCPP(self):
    lines = [
      f"// Generated Register Definitions for {self.namespace}",
      "#pragma once",
      "#include <cstdint>",
      ""
    ]

    # Emit Structs (with nested Enums)
    for reg in self.registers:
      if reg.doc:
        lines.append(f"// {reg.doc}")
      lines.append(f"struct __attribute__((packed, aligned(4))) {self.namespace}{reg.name} {{")

      # Nested Enums inside the register struct
      for f in reg.fields:
        if isinstance(f, Enum):
          typeName = f"{capFirst(f.name)}Enum" if len(f.values) == 1 and not f.values[0][0] else capFirst(f.name)
          lines.append(f"  enum {typeName}: uint32_t {{")

          for idx, (vName, vVal, vDoc) in enumerate(f.values):
            symName = f"{f.name}{capFirst(vName)}" if vName else f"e{capFirst(f.name)}"
            comma = "," if idx < len(f.values) - 1 else ""
            docComment = f" // {vDoc}" if vDoc else ""
            lines.append(f"    {symName} = 0x{vVal:X}{comma}{docComment}")

          lines.append("  };")
          lines.append("")

      lines.append("  union {")
      lines.append("    struct {")
      indent = "      "

      for f in reg.fields:
        typeStr = "int32_t" if f.signed else "uint32_t"

        if isinstance(f, Enum):
          typeStr = f"{capFirst(f.name)}Enum" if len(f.values) == 1 and not f.values[0][0] else capFirst(f.name)

        fName = f"reserved{f.offset}" if f.name == "reserved" else f.name
        lines.append(f"{indent}{typeStr} {fName}: {f.bits};")

      lines.append("    };")

      if reg.totalBits <= 8:
        uType = "uint8_t"
      elif reg.totalBits <= 16:
        uType = "uint16_t"
      elif reg.totalBits <= 32:
        uType = "uint32_t"
      else:
        uType = "uint64_t"

      lines.append(f"    {uType} u;")
      lines.append("  };")
      lines.append(f"  static constexpr unsigned bitW = {reg.totalBits};")
      lines.append("};")
      lines.append("")

    # Emit Address Enum
    lines.append(f"enum {self.namespace}Addr {{")
    for reg in self.registers:
      lines.append(f"  a{self.namespace}{reg.name} = 0x{reg.addr:02X},")
    lines.append(f"  w{self.namespace}Addr = 7")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)

  ################################################################
  def emitMD(self):
    lines = [f"# {self.namespace} Register Map", ""]

    for reg in self.registers:
      lines.append(f"## {reg.name} (Address: 0x{reg.addr:02X})")

      if reg.doc:
        lines.append(reg.doc)
        lines.append("")

      lines.append("### Bit Layout")
      lines.append("")
      header = "|"
      strip = "|"
      align = "|"

      for f in reversed(reg.fields):
        displayName = f"_reserved_[{f.bits}]" if f.name == "reserved" else f"{f.name}[{f.bits}]"
        bitRange = f"{f.offset + f.bits - 1}"
        if f.bits > 1:
          bitRange += f"..{f.offset}"

        width = max(len(f.name), len(bitRange), len(displayName))
        header += f" {bitRange:^{width}} |"
        strip  += f" {displayName:^{width}} |"
        align  += ":" + "-" * width + ":|"

      lines.append(header)
      lines.append(align)
      lines.append(strip)
      lines.append("")

      lines.append("| Bits | Field | Type | Default | Description |")
      lines.append("| :--- | :--- | :--- | :--- | :--- |")

      for f in reversed(reg.fields):
        bitRange = f"{f.offset + f.bits - 1}:{f.offset}" if f.bits > 1 else f"{f.offset}"
        fType = "UInt"
        defaultStr = f"0x{f.default:X}" if f.bits > 4 else f"{f.default}"
        descStr = f.doc

        if isinstance(f, Enum):
          fType = "Enum"
          defName = f.getDefaultName()
          if defName:
            symName = f"{f.name}{capFirst(defName)}"
            defaultStr = f"`{symName}` ({f.default})"
          enumParts = []
          for vName, vVal, vDoc in f.values:
            symName = f"{f.name}{capFirst(vName)}" if vName else f"e{capFirst(f.name)}"
            valStr = f"0x{vVal:X}" if f.bits > 4 else f"{vVal}"
            docPart = f" - {vDoc}" if vDoc else ""
            enumParts.append(f"`{symName}`: {valStr}{docPart}")
          if enumParts:
            sep = "<br>" if descStr else ""
            descStr = f"{descStr}{sep}**Values:** " + ", ".join(enumParts)
        elif isinstance(f, Bit):
          fType = "Bit"
        elif f.signed:
          fType = "Int"

        displayName = f"_reserved_[{f.bits}]" if f.name == "reserved" else f.name
        lines.append(f"| {bitRange} | {displayName} | {fType} | {defaultStr} | {descStr} |")

      lines.append("")
    return "\n".join(lines)

  ################################################################
  def writeFiles(self, prefix):
    with open(f"{prefix}.h", "w") as f:
      f.write(self.emitCPP())
    with open(f"{prefix}.md", "w") as f:
      f.write(self.emitMD())
    with open(f"{prefix}.sv", "w") as f:
      f.write(self.emitSV())

if __name__ == "__main__":
  print("Register Tool: Import this module in your definition script.")
