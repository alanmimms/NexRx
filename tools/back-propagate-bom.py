import csv
import re
import glob
import os

def expand_refs(ref_str):
    """Handles 'C1,C2', 'C1-C5', 'C1, C2-C4'."""
    refs = []
    # Split by comma first
    parts = [p.strip() for p in ref_str.split(',')]
    for p in parts:
        if '-' in p:
            # Range expansion: prefix + start_num + '-' + end_num
            # e.g. C203-C206
            m = re.match(r'^([A-Z]+)([0-9]+)-([0-9]+)$', p)
            if m:
                prefix, start, end = m.groups()
                for i in range(int(start), int(end) + 1):
                    refs.append(f"{prefix}{i}")
            else:
                # If it doesn't match the expected range format, keep as is
                refs.append(p)
        else:
            refs.append(p)
    return refs

def extract_properties(block):
    """Extracts property expressions from a symbol block with balanced parentheses."""
    properties = []
    pos = 0
    while True:
        # Find start of a property
        match = re.search(r'\(property\s+"([^"]+)"\s+"([^"]*)"', block[pos:])
        if not match:
            break
        
        p_start = pos + match.start()
        name = match.group(1)
        value = match.group(2)
        
        # Find balanced closing paren for this property
        p_stack = 1
        p_pos = p_start + 1
        while p_pos < len(block) and p_stack > 0:
            if block[p_pos] == '(':
                p_stack += 1
            elif block[p_pos] == ')':
                p_stack -= 1
            p_pos += 1
        
        p_expr = block[p_start:p_pos]
        properties.append({
            'name': name,
            'value': value,
            'expr': p_expr,
            'start': p_start,
            'end': p_pos
        })
        pos = p_pos
    return properties

def clean_and_update_block(block, props_from_bom, to_remove, to_add):
    """Removes Babel properties and updates/adds target properties."""
    current_props = extract_properties(block)
    
    new_values = {
        "Value": props_from_bom['Value'],
        "Manufacturer": props_from_bom['Manufacturer'],
        "MPN": props_from_bom['MPN'],
        "LCSC_PN": props_from_bom['LCSC_PN'],
        "Mouser_PN": props_from_bom['Mouser_PN'],
        "Description": props_from_bom['Description']
    }
    
    # Target properties to update (including Value)
    to_add_and_value = ["Value"] + to_add
    
    # Remove properties we want to collapse/update
    cleaned_block = block
    for p in sorted(current_props, key=lambda x: x['start'], reverse=True):
        if p['name'] in to_remove or p['name'] in ["Value"] or p['name'] in to_add:
            cleaned_block = cleaned_block[:p['start']] + cleaned_block[p['end']:]
            
    # Find insertion point: after Reference property
    remaining_props = extract_properties(cleaned_block)
    ref_prop = next((p for p in remaining_props if p['name'] == "Reference"), None)
    if ref_prop:
        insert_pos = ref_prop['end']
    else:
        # Fallback
        lib_id_match = re.search(r'\(lib_id\s+"[^"]+"\)', cleaned_block)
        insert_pos = lib_id_match.end() if lib_id_match else cleaned_block.find('\n') + 1

    insertion = ""
    for name in to_add_and_value:
        val = new_values[name]
        escaped_val = val.replace('\\', '\\\\').replace('"', '\\"')
        # Value property usually shouldn't be hidden, but other new ones should
        hide = " (hide yes)" if name != "Value" else ""
        insertion += f'\n    (property "{name}" "{escaped_val}" (at 0 0 0) (effects (font (size 1.27 1.27)){hide}))'
    
    return cleaned_block[:insert_pos] + insertion + cleaned_block[insert_pos:]

def main():
    # 1. Load BOM mapping
    bom_map = {}
    bom_path = 'production/nexrx-bom.csv'
    if not os.path.exists(bom_path):
        print(f"BOM not found at {bom_path}")
        return

    with open(bom_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            refs = expand_refs(row['Refs'])
            for r in refs:
                bom_map[r] = row

    # 2. Define property lists
    to_remove = [
        "Manufacturer_Name", "MF", "Mfg", "Mfr", "Vendor", 
        "Manufacturer_Part_Number", "MP", "MPN", "MfgPart", "P1", "P2", "P3", "P4",
        "LCSC", "LCSC_PN", "JLCPCBpn", "L1", "L2", "L3",
        "Mouser_PN", "Mouser Part Number", "MO1", "MO2"
    ]
    to_add = ["Manufacturer", "MPN", "LCSC_PN", "Mouser_PN", "Description"]

    # 3. Process each schematic file
    sch_files = glob.glob('hw/*.kicad_sch')
    for sch_path in sch_files:
        print(f"Processing {sch_path}...")
        with open(sch_path, 'r', encoding='utf-8') as f:
            content = f.read()

        new_content = ""
        last_end = 0
        pos = 0
        while pos < len(content):
            # Look for start of a symbol block
            start_match = re.search(r'\(symbol\s+', content[pos:])
            if not start_match:
                break
            
            start = pos + start_match.start()
            new_content += content[last_end:start]
            
            # Find balanced closing paren for this symbol
            stack = 1
            i = start + 1
            while i < len(content) and stack > 0:
                if content[i] == '(':
                    stack += 1
                elif content[i] == ')':
                    stack -= 1
                i += 1
            
            block = content[start:i]
            
            # Identify Reference designator
            # Note: We need the property "Reference" at the top level of this symbol block
            ref_match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
            if ref_match:
                ref = ref_match.group(1)
                if ref in bom_map:
                    block = clean_and_update_block(block, bom_map[ref], to_remove, to_add)
            
            new_content += block
            last_end = i
            pos = i
            
        new_content += content[last_end:]
        
        with open(sch_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

    print("Back-propagation complete.")

if __name__ == "__main__":
    main()
