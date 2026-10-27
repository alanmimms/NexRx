#!/usr/bin/env python3
import sys
import math
import argparse
from PIL import Image, ImageDraw, ImageFont

def printAvailableVariations(font, fontPath):
  sys.stderr.write(f"\nDiagnostic info for '{fontPath}':\n")
  
  # Print defined axes (e.g. wght, wdth) with minimum, default, and maximum ranges
  try:
    axes = font.get_variation_axes()
    if axes:
      sys.stderr.write("  Variation axes:\n")
      for ax in axes:
        # In Pillow, ax is typically a dict with 'name', 'minimum', 'default', 'maximum'
        name = ax.get("name", "unknown")
        minVal = ax.get("minimum", 0.0)
        defVal = ax.get("default", 0.0)
        maxVal = ax.get("maximum", 0.0)
        sys.stderr.write(f"    - tag: '{name}', range: [{minVal} .. {maxVal}], default: {defVal}\n")
    else:
      sys.stderr.write("  No variation axes exposed by FreeType.\n")
  except (AttributeError, IOError) as err:
    sys.stderr.write(f"  Unable to query variation axes: {err}\n")

  # Print named instances (e.g. 'Thin', 'Light', 'Regular', 'Bold')
  try:
    namedInstances = font.get_variation_names()
    if namedInstances:
      sys.stderr.write("  Named variation instances:\n")
      for item in namedInstances:
        # Depending on Pillow version, item is a string name or an object with a .name attribute
        instanceName = getattr(item, "name", str(item))
        sys.stderr.write(f"    - '{instanceName}'\n")
    else:
      sys.stderr.write("  No named variation instances found.\n")
  except (AttributeError, IOError) as err:
    sys.stderr.write(f"  Unable to query variation names: {err}\n")
  sys.stderr.write("\n")


def applyFontVariations(font, fontPath, weight=None, variation=None):
  if variation:
    try:
      font.set_variation_by_name(variation)
    except (IOError, ValueError) as err:
      sys.stderr.write(f"Error: Failed to set variation instance '{variation}': {err}\n")
      printAvailableVariations(font, fontPath)
      sys.exit(1)

  if weight is not None:
    try:
      axes = font.get_variation_axes()
      if not axes:
        raise ValueError("Font does not provide variation axes (not a variable font).")

      # Build axis coordinate vector, updating the 'wght' tag specifically
      axisValues = []
      foundWeightAxis = False
      for ax in axes:
        tag = ax.get("name", "")
        if tag == "wght":
          foundWeightAxis = True
          minVal = ax.get("minimum", 100.0)
          maxVal = ax.get("maximum", 900.0)
          clampedWeight = max(minVal, min(float(weight), maxVal))
          if clampedWeight != float(weight):
            sys.stderr.write(f"Notice: Clamping weight {weight} to supported range [{minVal} .. {maxVal}]\n")
          axisValues.append(clampedWeight)
        else:
          axisValues.append(float(ax.get("default", 100.0)))

      if not foundWeightAxis:
        raise ValueError("Font has variation axes, but no 'wght' axis was found.")

      font.set_variation_by_axes(axisValues)

    except (AttributeError, IOError, ValueError) as err:
      sys.stderr.write(f"Error: Failed setting weight '{weight}': {err}\n")
      printAvailableVariations(font, fontPath)
      sys.exit(1)

def generateFont(path, size, name="customFont", fontIndex=0, weight=None, variation=None):
  code = []

  try:
    font = ImageFont.truetype(path, size, index=fontIndex, layout_engine=ImageFont.Layout.BASIC)
  except IOError:
    print(f"Error: Could not load font '{path}' at index {fontIndex}.")
    sys.exit(1)

  applyFontVariations(font, path, weight=weight, variation=variation)

  # Vertical metrics to maintain baseline
  ascent, descent = font.getmetrics()
  totalH = ascent + descent
  
  # 8-pixel pages calculation
  hPages = math.ceil(totalH / 8.0)
  code.append(f"// hPages={hPages}")
  
  chars = [chr(i) for i in range(32, 127)]
  
  glyphs = []
  bits = []
  currentOffset = 0

  # Threshold (0-255): lower values make stems slightly bolder/fuller at small sizes
  pixelThreshold = 110

  # Dummy grayscale context so textbbox() uses the exact same renderer as draw.text()
  measureImg = Image.new('L', (1, 1), color=0)
  measureDraw = ImageDraw.Draw(measureImg)

  for c in chars:
    advance = int(math.ceil(font.getlength(c)))
    if advance == 0:
      advance = int(size / 3)

    # Measure in 'L' mode anchored to Left-Baseline ("ls") at Y = ascent
    bbox = measureDraw.textbbox((0, ascent), c, font=font, anchor="ls")

    if bbox is None:
      inkW = 0
      left = 0
    else:
      left, top, right, bottom = bbox
      inkW = max(0, right - left)

    if inkW > 0:
      # Render in 8-bit grayscale ('L') to avoid broken 1-bit TrueType bytecode hints
      img = Image.new('L', (inkW, totalH), color=0)
      draw = ImageDraw.Draw(img)

      # Draw anchored at the baseline (Y = ascent)
      draw.text((-left, ascent), c, font=font, fill=255, anchor="ls")
      pixels = img.load()

      # Convert to MONO_VTILED: Page by Page, Column by Column
      for p in range(hPages):
        for x in range(inkW):
          byte = 0
          for bit in range(8):
            y = p * 8 + bit
            if y < totalH and pixels[x, y] >= pixelThreshold:
              byte |= (1 << bit)
          bits.append(byte)  
  
    glyphs.append({
      'char': c,
      'offset': currentOffset,
      'inkW': inkW,
      'advance': advance
    })
    currentOffset += (inkW * hPages)

  code.append("/* Automatically generated by ttf-to-cxx.py */")
  code.append("#include <FrameBuffer.h>\n")

  code.append(f"static const std::vector<FrameBuffer::Glyph> {name}Glyphs {{")
  for g in glyphs:
    ch = g['char'].replace('\\', '\\\\').replace("'", "\\'")
    code.append(f"  {{ {g['offset']:5d}, {g['inkW']:3d}, {g['advance']:3d} }}, // '{ch}'")
  code.append("};\n")

  code.append(f"static const uint8_t {name}Bits[] = {{")
  for i in range(0, len(bits), 12):
    chunk = bits[i:i+12]
    hexStr = ", ".join([f"0x{b:02X}" for b in chunk])
    code.append(f"  {hexStr},")
  code.append("};\n")

  code.append(f"extern const FrameBuffer::Font {name}({hPages}, {name}Glyphs, {name}Bits);")
  return "\n".join(code)

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Convert TTF/OTF/TTC to MONO_VTILED C++ font.")
  parser.add_argument("path", help="Path to TTF/OTF/TTC font file")
  parser.add_argument("size", type=int, help="Font size in pixels")
  parser.add_argument("--name", default="customFont", help="C++ variable name")
  parser.add_argument("--index", type=int, default=0, help="Font index inside a TTC/OTC collection")
  parser.add_argument("--weight", type=int, default=None, help="Variable font weight value (e.g. 100=Thin, 300=Light, 700=Bold, 900=Black)")
  parser.add_argument("--variation", default=None, help="Named variation instance (e.g. 'Bold', 'Light')")
  args = parser.parse_args()

  source = generateFont(
    args.path,
    args.size,
    name=args.name,
    fontIndex=args.index,
    weight=args.weight,
    variation=args.variation
  )
  print(source)
