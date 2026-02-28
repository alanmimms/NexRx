import csv
import re
import glob
import os
import subprocess

def expand_refs(ref_str):
    refs = []
    parts = [p.strip() for p in ref_str.split(',')]
    for p in parts:
        if '-' in p:
            m = re.match(r'^([A-Z]+)([0-9]+)-([0-9]+)$', p)
            if m:
                prefix, start, end = m.groups()
                for i in range(int(start), int(end) + 1):
                    refs.append(f"{prefix}{i}")
            else:
                refs.append(p)
        else:
            refs.append(p)
    return refs

def extract_properties_with_meta(block):
    """Extracts property dict with meta info like 'at' and 'effects'."""
    props = {}
    pos = 0
    while True:
        match = re.search(r'\(property\s+"([^"]+)"\s+"([^"]*)"', block[pos:])
        if not match:
            break
        
        p_start = pos + match.start()
        name = match.group(1)
        value = match.group(2)
        
        # Find balanced closing paren
        p_stack = 1
        p_pos = p_start + 1
        while p_pos < len(block) and p_stack > 0:
            if block[p_pos] == '(': p_stack += 1
            elif block[p_pos] == ')': p_stack -= 1
            p_pos += 1
        
        p_expr = block[p_start:p_pos]
        
        # Extract (at ...) and (effects ...)
        at_match = re.search(r'\(at\s+[^)]+\)', p_expr)
        effects_match = re.search(r'\(effects\s+.*?\)\s*\)', p_expr, re.DOTALL) or re.search(r'\(effects\s+.*?\)', p_expr, re.DOTALL)
        
        # More robust effects extraction
        effects_block = ""
        e_m = re.search(r'\(effects\s+', p_expr)
        if e_m:
            e_start = e_m.start()
            e_stack = 1
            e_pos = e_start + 1
            while e_pos < len(p_expr) and e_stack > 0:
                if p_expr[e_pos] == '(': e_stack += 1
                elif p_expr[e_pos] == ')': e_stack -= 1
                e_pos += 1
            effects_block = p_expr[e_start:e_pos]

        props[name] = {
            'value': value,
            'at': at_match.group(0) if at_match else '(at 0 0 0)',
            'effects': effects_block if effects_block else '(effects (font (size 1.27 1.27)))',
            'start': p_start,
            'end': p_pos
        }
        pos = p_pos
    return props

def get_symbols_from_content(content):
    symbols = []
    pos = 0
    while pos < len(content):
        start_match = re.search(r'\(symbol\s+', content[pos:])
        if not start_match: break
        start = pos + start_match.start()
        stack = 1
        i = start + 1
        while i < len(content) and stack > 0:
            if content[i] == '(': stack += 1
            elif content[i] == ')': stack -= 1
            i += 1
        block = content[start:i]
        
        ref_match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
        if ref_match:
            symbols.append({'ref': ref_match.group(1), 'block': block, 'start': start, 'end': i})
        pos = i
    return symbols

def main():
    bom_map = {}
    with open('production/nexrx-bom.csv', 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            for r in expand_refs(row['Refs']): bom_map[r] = row

    to_remove = [
        "Manufacturer_Name", "MF", "Mfg", "Mfr", "Vendor", 
        "Manufacturer_Part_Number", "MP", "MPN", "MfgPart", "P1", "P2", "P3", "P4",
        "LCSC", "LCSC_PN", "JLCPCBpn", "L1", "L2", "L3",
        "Mouser_PN", "Mouser Part Number", "MO1", "MO2"
    ]
    to_add = ["Manufacturer", "MPN", "LCSC_PN", "Mouser_PN", "Description"]

    for sch_path in glob.glob('hw/*.kicad_sch'):
        print(f"Processing {sch_path}...")
        
        # Get current content
        with open(sch_path, 'r', encoding='utf-8') as f:
            current_content = f.read()
            
        # Get old content from Git (HEAD~1)
        try:
            old_content = subprocess.check_output(['git', 'show', 'HEAD~1:' + sch_path], stderr=subprocess.STDOUT).decode('utf-8')
        except subprocess.CalledProcessError:
            print(f"  Skipping {sch_path}, not found in previous commit.")
            continue

        old_symbols = get_symbols_from_content(old_content)
        old_sym_map = {s['ref']: s for s in old_symbols}

        new_content = ""
        last_end = 0
        pos = 0
        while pos < len(current_content):
            start_match = re.search(r'\(symbol\s+', current_content[pos:])
            if not start_match: break
            start = pos + start_match.start()
            new_content += current_content[last_end:start]
            stack = 1
            i = start + 1
            while i < len(current_content) and stack > 0:
                if current_content[i] == '(': stack += 1
                elif current_content[i] == ')': stack -= 1
                i += 1
            block = current_content[start:i]
            
            ref_match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
            if ref_match:
                ref = ref_match.group(1)
                if ref in bom_map:
                    # Found component to update
                    props_from_bom = bom_map[ref]
                    old_props = {}
                    if ref in old_sym_map:
                        old_props = extract_properties_with_meta(old_sym_map[ref]['block'])
                    
                    current_props_list = []
                    # We need to extract properties from current block to know where to insert
                    # but we'll rebuild them.
                    
                    # Target values
                    new_values = {
                        "Value": props_from_bom['Value'],
                        "Manufacturer": props_from_bom['Manufacturer'],
                        "MPN": props_from_bom['MPN'],
                        "LCSC_PN": props_from_bom['LCSC_PN'],
                        "Mouser_PN": props_from_bom['Mouser_PN'],
                        "Description": props_from_bom['Description']
                    }

                    # Extract properties from CURRENT block to keep ones we don't touch (Reference, Footprint, etc)
                    curr_props = extract_properties_with_meta(block)
                    
                    # Identify Reference property to use as anchor
                    ref_p = curr_props.get("Reference")
                    
                    # Build new properties
                    new_block = block
                    # Delete all properties that we are updating or that should be removed
                    # Sort by reverse index to not break offsets
                    for p_name, p_info in sorted(curr_props.items(), key=lambda x: x[1]['start'], reverse=True):
                        if p_name in to_remove or p_name in to_add or p_name == "Value":
                            new_block = new_block[:p_info['start']] + new_block[p_info['end']:]
                    
                    # Find insertion point: after Reference property
                    # Re-extract to get new offsets
                    temp_props = extract_properties_with_meta(new_block)
                    ref_p = temp_props.get("Reference")
                    insert_pos = ref_p['end'] if ref_p else new_block.find('\n') + 1
                    
                    insertion = ""
                    # Add Value first (anchor it to old position if possible)
                    v_val = new_values["Value"]
                    v_at = old_props.get("Value", {}).get("at", "(at 0 0 0)")
                    v_eff = old_props.get("Value", {}).get("effects", "(effects (font (size 1.27 1.27)))")
                    insertion += f'\n    (property "Value" "{v_val}" {v_at} {v_eff})'
                    
                    # Add others
                    for name in to_add:
                        val = new_values[name].replace('\\', '\\\\').replace('"', '\\"')
                        # Try to get old position if it existed, otherwise hidden default
                        at = old_props.get(name, {}).get("at", "(at 0 0 0)")
                        eff = old_props.get(name, {}).get("effects", "(effects (font (size 1.27 1.27)) (hide yes))")
                        # Ensure hidden if it's metadata
                        if name != "Description" and "(hide yes)" not in eff:
                            eff = eff.replace("(effects", "(effects (hide yes)")
                        insertion += f'\n    (property "{name}" "{val}" {at} {eff})'
                    
                    block = new_block[:insert_pos] + insertion + new_block[insert_pos:]

            new_content += block
            last_end = i
            pos = i
            
        new_content += current_content[last_end:]
        with open(sch_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

    print("Back-propagation and position recovery complete.")

if __name__ == "__main__":
    main()
