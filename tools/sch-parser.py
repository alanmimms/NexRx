import re
import os
import glob
import csv

def get_sexpr_block(content, start_idx):
    stack = 0
    for i in range(start_idx, len(content)):
        if content[i] == '(':
            stack += 1
        elif content[i] == ')':
            stack -= 1
            if stack == 0:
                return content[start_idx:i+1], i+1
    return None, None

def parse_kicad_sch(sch_path):
    print(f"Parsing {sch_path}...")
    with open(sch_path, 'r', encoding='utf-8') as f:
        content = f.read()

    symbols = []
    idx = 0
    while True:
        match = re.search(r'\(symbol\s+', content[idx:])
        if not match:
            break
        
        start_pos = idx + match.start()
        remaining = content[start_pos + 8:].lstrip()
        if not remaining.startswith('('):
            _, next_idx = get_sexpr_block(content, start_pos)
            idx = next_idx if next_idx else start_pos + 8
            continue

        block, next_idx = get_sexpr_block(content, start_pos)
        if not block: break
        idx = next_idx
        
        # Check if it's (symbol (lib_id ...)
        if not re.search(r'^\(symbol\s+\(lib_id\s+', block):
            continue

        uuid_match = re.search(r'\(uuid\s+"?([a-f0-9-]{36})"?\)', block)
        if not uuid_match: continue
        symbol_uuid = uuid_match.group(1)

        val_match = re.search(r'\(property\s+"Value"\s+"([^"]*)"', block)
        fp_match = re.search(r'\(property\s+"Footprint"\s+"([^"]*)"', block)
        value = val_match.group(1) if val_match else ""
        footprint = fp_match.group(1) if fp_match else ""

        instances_match = re.search(r'\(instances\s+', block)
        if instances_match:
            inst_block, _ = get_sexpr_block(block, instances_match.start())
            if inst_block:
                paths = re.findall(r'\(path\s+"([^"]+)"\s+\(reference\s+"([^"]+)"\)', inst_block)
                for path, ref in paths:
                    symbols.append({
                        "UUID": symbol_uuid,
                        "Path": path,
                        "Reference": ref,
                        "Value": value,
                        "Footprint": footprint,
                        "File": os.path.basename(sch_path)
                    })
        else:
            ref_match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
            if ref_match:
                symbols.append({
                    "UUID": symbol_uuid,
                    "Path": "",
                    "Reference": ref_match.group(1),
                    "Value": value,
                    "Footprint": footprint,
                    "File": os.path.basename(sch_path)
                })
                
    print(f"Found {len(symbols)} symbols in {sch_path}")
    return symbols

def main():
    all_symbols = []
    for sch_file in glob.glob("hw/*.kicad_sch"):
        all_symbols.extend(parse_kicad_sch(sch_file))
    
    all_symbols.sort(key=lambda x: x['Reference'])
    
    output_path = "hw/production/component_map.csv"
    with open(output_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["Reference", "UUID", "Path", "Value", "Footprint", "File"])
        writer.writeheader()
        writer.writerows(all_symbols)
    
    print(f"Extracted {len(all_symbols)} symbol instances to {output_path}")

if __name__ == "__main__":
    main()
