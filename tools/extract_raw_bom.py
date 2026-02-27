import re
import os
import glob
import csv

def parse_schematic(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Find all symbols
    # Symbols start with (symbol ... and end with a matching )
    # This regex is a bit simplistic for nested parens, but KiCad symbols are usually flat-ish in structure for properties
    symbol_blocks = []
    stack = 0
    start = -1
    for i, char in enumerate(content):
        if content[i:i+8] == '(symbol ':
            if stack == 0:
                start = i
            stack += 1
        elif char == '(':
            if stack > 0:
                stack += 1
        elif char == ')':
            if stack > 0:
                stack -= 1
                if stack == 0:
                    symbol_blocks.append(content[start:i+1])

    components = []
    for block in symbol_blocks:
        # Extract properties
        def get_prop(name):
            match = re.search(r'\(property\s+"' + re.escape(name) + r'"\s+"([^"]*)"', block)
            return match.group(1) if match else None

        ref = get_prop("Reference")
        # Filter out power symbols, test points, etc. if needed, but for now let's get everything
        if not ref or ref.startswith('#') or ref == 'GND' or ref == 'PWR':
            continue

        val = get_prop("Value")
        foot = get_prop("Footprint")
        cap_type = get_prop("cap-type")
        working_voltage = get_prop("working-voltage")
        description = get_prop("Description")
        
        # Also look for LCSC or MPN properties that might already exist
        lcsc = get_prop("LCSC") or get_prop("LCSC_PN") or get_prop("JLCPCBpn")
        mpn = get_prop("MPN") or get_prop("MP") or get_prop("Manufacturer_Part_Number")

        components.append({
            'ref': ref,
            'value': val,
            'footprint': foot,
            'cap_type': cap_type,
            'working_voltage': working_voltage,
            'description': description,
            'lcsc': lcsc,
            'mpn': mpn,
            'file': os.path.basename(file_path)
        })

    return components

def main():
    sch_files = [
        "hw/NexRx.kicad_sch",
        "hw/rx-signal-chain.kicad_sch",
        "hw/fpga.kicad_sch",
        "hw/microcontroller.kicad_sch",
        "hw/power.kicad_sch",
        "hw/rx-presel.kicad_sch",
        "hw/qsd.kicad_sch",
        "hw/PGA.kicad_sch",
        "hw/tpad.kicad_sch",
        "hw/gpio-expander.kicad_sch"
    ]

    all_components = []
    for f in sch_files:
        if os.path.exists(f):
            all_components.extend(parse_schematic(f))

    # Output raw extraction for review/further processing
    with open('production/raw_components.csv', 'w', newline='') as csvfile:
        fieldnames = ['ref', 'value', 'footprint', 'cap_type', 'working_voltage', 'lcsc', 'mpn', 'file']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for comp in all_components:
            writer.writerow({k: comp.get(k, '') for k in fieldnames})

    print(f"Extracted {len(all_components)} components.")

if __name__ == "__main__":
    main()
