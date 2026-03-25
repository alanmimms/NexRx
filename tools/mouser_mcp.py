import os
import requests
import csv
import re
import glob
import subprocess
import time
from mcp.server.fastmcp import FastMCP

# Working Mouser API Key provided by user
MOUSER_API_KEY = "11d084b4-f381-46c0-8730-9b8369e083bd"
MOUSER_BASE_URL = "https://api.mouser.com/api/v2"

mcp = FastMCP("Mouser-BOM-Manager")

def parse_voltage(v_str):
    """Converts voltage strings like '50V', '6.3V' to float."""
    if not v_str: return 0.0
    m = re.search(r'([0-9.]+)', v_str)
    return float(m.group(1)) if m else 0.0

def simplify_footprint(fp):
    """Extract standard SMD sizes like 0402, 0603, etc."""
    m = re.search(r'(0201|0402|0603|0805|1206|1210|3216|3528)', fp)
    if m:
        return m.group(1)
    parts = fp.split(':')
    return parts[-1] if len(parts) > 1 else fp

def call_mouser_search(query):
    url = f"{MOUSER_BASE_URL}/search/keyword?apiKey={MOUSER_API_KEY}"
    payload = {"SearchByKeywordRequest": {"keyword": query, "records": 10, "startingRecord": 0, "searchOptions": "None"}}
    headers = {"Content-Type": "application/json", "Accept": "application/json"}
    try:
        response = requests.post(url, json=payload, headers=headers)
        response.raise_for_status()
        return response.json()
    except Exception as e:
        print(f"\nAPI Error (Keyword): {e}")
        return {"Error": str(e)}

def call_mouser_part_search(mpn):
    url = f"{MOUSER_BASE_URL}/search/partnumber?apiKey={MOUSER_API_KEY}"
    payload = {"SearchByPartRequest": {"mouserPartNumber": mpn, "partSearchOptions": ""}}
    headers = {"Content-Type": "application/json", "Accept": "application/json"}
    try:
        response = requests.post(url, json=payload, headers=headers)
        response.raise_for_status()
        return response.json()
    except Exception as e:
        print(f"\nAPI Error (Part): {e}")
        return {"Error": str(e)}

def extract_schematic_parts():
    fields = ["Reference", "Value", "Footprint", "Manufacturer", "Manufacturer_Name", "Manufacturer_Part_Number", "MPN", "Mouser_PN", "Mouser Part Number", "cap-type", "working-voltage", "${QUANTITY}"]
    labels = ["Ref", "Value", "Footprint", "M1", "M2", "P1", "P2", "MO1", "MO2", "CapType", "Voltage", "Qty"]
    output_file = "hw/production/raw_schematic_parts.csv"
    cmd = ["kicad-cli", "sch", "export", "bom", "--fields", ",".join(fields), "--labels", ",".join(labels), "--output", output_file, "hw/NexRx.kicad_sch"]
    subprocess.run(cmd, check=True)
    parts = []
    with open(output_file, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            parts.append(row)
    return parts

@mcp.tool()
def verify_and_consolidate_bom():
    """
    Groups parts, finds best Mouser matches, and performs cost-based consolidation.
    """
    raw_parts = extract_schematic_parts()
    
    # 1. Group by equivalence key: (Value, Footprint, CapType/Composition)
    # We treat resistors as one group type, capacitors as another.
    groups = {}
    for p in raw_parts:
        ref = p['Ref']
        val = p['Value']
        fp = simplify_footprint(p['Footprint'])
        v_req = parse_voltage(p['Voltage'])
        ctype = p['CapType'] or ""
        
        # Explicit part numbers bypass consolidation grouping for now
        explicit_mpn = p['P1'] or p['P2'] or p['MO1'] or p['MO2']
        
        if explicit_mpn:
            key = f"EXPLICIT:{explicit_mpn}"
        elif ref.startswith('C'):
            key = f"CAP:{val}:{fp}:{ctype.upper()}"
        elif ref.startswith('R'):
            key = f"RES:{val}:{fp}"
        else:
            key = f"OTHER:{val}:{fp}"
            
        if key not in groups:
            groups[key] = {'parts': [], 'max_v': 0.0, 'val': val, 'fp': fp, 'ctype': ctype, 'explicit': explicit_mpn}
        
        groups[key]['parts'].append(p)
        groups[key]['max_v'] = max(groups[key]['max_v'], v_req)

    consolidated_bom = []
    print(f"Processing {len(groups)} part groups...")

    for i, (key, data) in enumerate(groups.items()):
        qty = sum(int(p['Qty']) for p in data['parts'])
        refs = ",".join(p['Ref'] for p in data['parts'])
        
        print(f"[{i+1}/{len(groups)}] Processing {refs[:20]}... ", end="", flush=True)
        
        search_query = ""
        result = None
        
        if data['explicit']:
            result = call_mouser_part_search(data['explicit'])
        else:
            if key.startswith('CAP'):
                search_query = f"Capacitor {data['ctype']} {data['val']} {data['max_v']}V {data['fp']}"
            elif key.startswith('RES'):
                search_query = f"Resistor 1% thin film {data['val']} {data['fp']}"
            else:
                search_query = f"{data['val']} {data['fp']}"
            result = call_mouser_search(search_query)

        best_part = None
        if result and result.get("SearchResults") and result["SearchResults"].get("Parts"):
            # Filter for voltage >= max_v and find cheapest
            parts_found = result["SearchResults"]["Parts"]
            candidates = []
            for pf in parts_found:
                p_v = parse_voltage(pf.get("Description", "")) # Fallback to description
                # More robust voltage check could be done via attributes
                if p_v >= data['max_v'] or not data['max_v']:
                    candidates.append(pf)
            
            if candidates:
                # Sort by price at group quantity
                def get_price(p_obj, target_qty):
                    breaks = p_obj.get("PriceBreaks", [])
                    price = 999999.0
                    for b in breaks:
                        if int(b.get("Quantity", 0)) <= target_qty:
                            p_str = b.get("Price", "$999999").replace('$', '').replace(',', '')
                            price = float(p_str)
                    return price
                
                candidates.sort(key=lambda x: get_price(x, qty))
                best_part = candidates[0]

        if best_part:
            price_breaks = best_part.get('PriceBreaks', [])
            price = price_breaks[0].get('Price', 'N/A') if price_breaks else 'N/A'
            note = f"Stock: {best_part.get('Availability', 'N/A')}, Price: {price}"
            if not data['explicit'] and key.startswith('CAP'):
                note += f" (Consolidated to {data['max_v']}V)"
            
            consolidated_bom.append({
                "Refs": refs,
                "Qty": qty,
                "Value": data['val'],
                "Footprint": data['fp'],
                "Required_V": data['max_v'],
                "Suggested_MPN": best_part.get("ManufacturerPartNumber", ""),
                "Suggested_Mouser_PN": best_part.get("MouserPartNumber", ""),
                "Notes": note
            })
            print("Found.")
        else:
            consolidated_bom.append({
                "Refs": refs,
                "Qty": qty,
                "Value": data['val'],
                "Footprint": data['fp'],
                "Required_V": data['max_v'],
                "Suggested_MPN": "",
                "Suggested_Mouser_PN": "",
                "Notes": "Not found on Mouser"
            })
            print("Not found.")
            
        time.sleep(3.0)

    with open("hw/production/consolidated_mouser_bom.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["Refs", "Qty", "Value", "Footprint", "Required_V", "Suggested_MPN", "Suggested_Mouser_PN", "Notes"])
        writer.writeheader()
        writer.writerows(consolidated_bom)

    return f"Consolidated BOM generated: hw/production/consolidated_mouser_bom.csv with {len(consolidated_bom)} line items."

@mcp.tool()
def update_schematic_from_bom():
    """Updates schematics using consolidated_mouser_bom.csv."""
    bom_path = "hw/production/consolidated_mouser_bom.csv"
    if not os.path.exists(bom_path): return "Error: consolidated_mouser_bom.csv not found."
    
    bom_updates = {}
    with open(bom_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for r in reader:
            if r['Suggested_MPN'] or r['Suggested_Mouser_PN']:
                for ref in r['Refs'].split(','):
                    bom_updates[ref] = {"MPN": r['Suggested_MPN'], "Mouser_PN": r['Suggested_Mouser_PN']}

    sch_files = glob.glob("hw/*.kicad_sch")
    updated_count = 0
    for ref, data in bom_updates.items():
        for sch in sch_files:
            if update_kicad_sch_property(sch, ref, "MPN", data['MPN']):
                update_kicad_sch_property(sch, ref, "Mouser_PN", data['Mouser_PN'])
                updated_count += 1
                break
    return f"Updated {updated_count} symbols in schematics."

def update_kicad_sch_property(sch_path, ref, name, value):
    with open(sch_path, 'r', encoding='utf-8') as f:
        content = f.read()
    pos = 0
    while True:
        symbol_match = re.search(r'\(symbol\s+', content[pos:])
        if not symbol_match: break
        start = pos + symbol_match.start()
        stack, i = 1, start + 1
        while i < len(content) and stack > 0:
            if content[i] == '(': stack += 1
            elif content[i] == ')': stack -= 1
            i += 1
        block = content[start:i]
        if re.search(r'\(property\s+"Reference"\s+"' + ref + r'"', block):
            prop_regex = r'\(property\s+"' + name + r'"\s+"([^"]*)"'
            prop_match = re.search(prop_regex, block)
            if prop_match:
                p_start = prop_match.start()
                p_stack, p_i = 1, p_start + 1
                while p_i < len(block) and p_stack > 0:
                    if block[p_i] == '(': p_stack += 1
                    elif block[p_i] == ')': p_stack -= 1
                    p_i += 1
                at_eff_match = re.search(r'\(at\s+[^)]+\)\s+\(effects\s+.*?\)\s*\)', block[p_start:p_i], re.DOTALL) or re.search(r'\(at\s+[^)]+\)\s+\(effects\s+.*?\)', block[p_start:p_i], re.DOTALL)
                at_eff = at_eff_match.group(0) if at_eff_match else ""
                updated_prop = f'(property "{name}" "{value}" {at_eff})'
                new_block = block[:p_start] + updated_prop + block[p_i:]
            else:
                ref_match = re.search(r'\(property\s+"Reference"\s+"' + ref + r'"', block)
                p_stack, p_i = 1, ref_match.end()
                while p_i < len(block) and p_stack > 0:
                    if block[p_i] == '(': p_stack += 1
                    elif block[p_i] == ')': p_stack -= 1
                    p_i += 1
                new_block = block[:p_i] + f'\n    (property "{name}" "{value}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))' + block[p_i:]
            content = content[:start] + new_block + content[i:]
            with open(sch_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        pos = i
    return False

if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        if sys.argv[1] == "verify": print(verify_and_consolidate_bom())
        elif sys.argv[1] == "update": print(update_schematic_from_bom())
    else: mcp.run()
