import csv
import re
import os
import glob

def update_kicad_sch_property(sch_path, ref, name, value):
    if not value: return False
    with open(sch_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # This is a simplified parser for KiCad 6.0+ s-expression format
    # It looks for (symbol ... (property "Reference" "REF") ... (property "NAME" "OLD_VALUE") ...)
    
    pos = 0
    modified = False
    while True:
        symbol_match = re.search(r'\(symbol\s+', content[pos:])
        if not symbol_match: break
        
        start = pos + symbol_match.start()
        # Find the end of this (symbol ...) block by counting parentheses
        stack = 1
        i = start + symbol_match.end()
        while i < len(content) and stack > 0:
            if content[i] == '(': stack += 1
            elif content[i] == ')': stack -= 1
            i += 1
        block_end = i
        block = content[start:block_end]
        
        # Check if this block is for our reference
        if re.search(r'\(property\s+"Reference"\s+"' + ref + r'"', block):
            # Try to find existing property
            prop_regex = r'\(property\s+"' + name + r'"\s+"([^"]*)"'
            prop_match = re.search(prop_regex, block)
            
            if prop_match:
                # Update existing property
                p_start = prop_match.start()
                # Find end of this property expression
                p_stack = 1
                p_i = p_start + 1
                while p_i < len(block) and p_stack > 0:
                    if block[p_i] == '(': p_stack += 1
                    elif block[p_i] == ')': p_stack -= 1
                    p_i += 1
                
                # Keep the (at ...) and (effects ...) if they exist
                at_eff_match = re.search(r'\(at\s+[^)]+\)\s+\(effects\s+[^)]+\)', block[p_start:p_i])
                at_eff = at_eff_match.group(0) if at_eff_match else '(at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes))'
                
                new_prop = f'(property "{name}" "{value}" {at_eff})'
                new_block = block[:p_start] + new_prop + block[p_i:]
                content = content[:start] + new_block + content[block_end:]
                modified = True
            else:
                # Add new property after the Reference property
                ref_prop_match = re.search(r'\(property\s+"Reference"\s+"' + ref + r'"\s+\(at\s+[^)]+\)\s+\(effects\s+[^)]+\)\s*\)', block)
                if ref_prop_match:
                    insert_pos = ref_prop_match.end()
                    new_prop = f'\n      (property "{name}" "{value}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'
                    new_block = block[:insert_pos] + new_prop + block[insert_pos:]
                    content = content[:start] + new_block + content[block_end:]
                    modified = True
            
            if modified:
                with open(sch_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                return True
        
        pos = block_end

    return False

def main():
    bom_path = "production/consolidated_bom.csv"
    if not os.path.exists(bom_path):
        print(f"Error: {bom_path} not found.")
        return

    sch_files = glob.glob("hw/*.kicad_sch")
    updated_count = 0

    with open(bom_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            refs = row['Refs'].split(',')
            mpn = row['MPN']
            mouser = row['Mouser_PN']
            mfr = row['Mfr']
            
            for ref in refs:
                ref = ref.strip()
                found = False
                for sch in sch_files:
                    if update_kicad_sch_property(sch, ref, "Manufacturer_Part_Number", mpn):
                        update_kicad_sch_property(sch, ref, "Mouser_PN", mouser)
                        update_kicad_sch_property(sch, ref, "Manufacturer_Name", mfr)
                        found = True
                        updated_count += 1
                        break
                if not found:
                    print(f"Warning: Reference {ref} not found in any schematic.")

    print(f"Successfully updated properties for {updated_count} components.")

if __name__ == "__main__":
    main()
