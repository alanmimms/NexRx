import csv
import re
import os
import subprocess
import time
import requests

# Mouser API Configuration
MOUSER_API_KEY = "11d084b4-f381-46c0-8730-9b8369e083bd"
MOUSER_BASE_URL = "https://api.mouser.com/api/v2"

def call_mouser_search(query):
    # Sanitize query: remove special characters that might cause 400 errors
    query = re.sub(r'[^\x00-\x7F]+', ' ', query) # Remove non-ASCII
    query = re.sub(r'["\+±]', ' ', query)       # Remove specific problematic chars
    query = ' '.join(query.split())               # Normalize whitespace

    url = f"{MOUSER_BASE_URL}/search/keyword?apiKey={MOUSER_API_KEY}"
    payload = {"SearchByKeywordRequest": {"keyword": query, "records": 10, "startingRecord": 0, "searchOptions": "None"}}
    headers = {"Content-Type": "application/json", "Accept": "application/json"}
    
    max_retries = 5
    delay = 5.0
    
    for attempt in range(max_retries):
        try:
            response = requests.post(url, json=payload, headers=headers)
            if response.status_code == 403 or response.status_code == 429:
                print(f" Rate limited (Status {response.status_code}). Retrying in {delay}s...")
                time.sleep(delay)
                delay *= 2
                continue
            
            response.raise_for_status()
            return response.json()
        except Exception as e:
            if attempt == max_retries - 1:
                print(f"\nAPI Error after {max_retries} attempts: {e}")
                return None
            time.sleep(1.0)
    return None

def call_mouser_part_search(mpn):
    url = f"{MOUSER_BASE_URL}/search/partnumber?apiKey={MOUSER_API_KEY}"
    payload = {"SearchByPartRequest": {"mouserPartNumber": mpn, "partSearchOptions": ""}}
    headers = {"Content-Type": "application/json", "Accept": "application/json"}
    try:
        response = requests.post(url, json=payload, headers=headers)
        if response.status_code == 403 or response.status_code == 429:
            print(f" Rate limited (Part Search).")
            return None
        response.raise_for_status()
        return response.json()
    except Exception as e:
        print(f"\nAPI Error (Part): {e}")
        return None

def parse_voltage(v_str):
    if not v_str: return 0.0
    # Common format like "50 V" or "50V"
    m = re.search(r'([0-9.]+)\s*[Vv]', v_str)
    if m: return float(m.group(1))
    # Fallback to any number if no V suffix
    m = re.search(r'([0-9.]+)', v_str)
    return float(m.group(1)) if m else 0.0

def simplify_footprint(fp):
    m = re.search(r'(0201|0402|0603|0805|1206|1210|3216|3528)', fp)
    if m: return m.group(1)
    parts = fp.split(':')
    return parts[-1] if len(parts) > 1 else fp

def get_price(p_obj, target_qty):
    breaks = p_obj.get("PriceBreaks", [])
    price = 999999.0
    for b in breaks:
        try:
            qty_break = int(b.get("Quantity", 0))
            if qty_break <= target_qty:
                p_str = b.get("Price", "$999999").replace('$', '').replace(',', '')
                price = float(p_str)
        except:
            continue
    return price

def extract_schematic_parts():
    fields = [
        "Reference", "Value", "Footprint", "Description",
        "Manufacturer", "Manufacturer_Name", "Manufacturer_Part_Number", "MPN",
        "Mouser_PN", "Mouser Part Number", "cap-type", "working-voltage", "${QUANTITY}"
    ]
    labels = [
        "Ref", "Value", "Footprint", "Description",
        "M1", "M2", "P1", "P2", "MO1", "MO2", "CapType", "Voltage", "Qty"
    ]
    output_file = "hw/production/raw_schematic_parts.csv"
    cmd = ["kicad-cli", "sch", "export", "bom", "--fields", ",".join(fields), "--labels", ",".join(labels), "--output", output_file, "hw/NexRx.kicad_sch"]
    subprocess.run(cmd, check=True)
    
    parts = []
    with open(output_file, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            parts.append(row)
    return parts

def consolidate_bom():
    raw_parts = extract_schematic_parts()
    groups = {}

    for p in raw_parts:
        ref = p['Ref']
        val = p['Value']
        fp = simplify_footprint(p['Footprint'])
        v_req = parse_voltage(p['Voltage'])
        ctype = p['CapType'] or ""
        mfr_name = p['M1'] or p['M2'] or ""
        mfr_part = p['P1'] or p['P2'] or ""
        desc = p['Description'] or ""

        if ref.startswith('C'):
            group_key = f"CAP:{val}:{fp}:{ctype.upper()}"
        elif ref.startswith('R'):
            group_key = f"RES:{val}:{fp}"
        else:
            group_key = f"OTHER:{val}:{fp}:{mfr_part}:{mfr_name}"

        if group_key not in groups:
            groups[group_key] = {
                'parts': [],
                'max_v': 0.0,
                'val': val,
                'fp': fp,
                'ctype': ctype,
                'mfr_names': set(),
                'mfr_parts': set(),
                'descriptions': set()
            }
        
        groups[group_key]['parts'].append(p)
        groups[group_key]['max_v'] = max(groups[group_key]['max_v'], v_req)
        if mfr_name: groups[group_key]['mfr_names'].add(mfr_name)
        if mfr_part: groups[group_key]['mfr_parts'].add(mfr_part)
        if desc: groups[group_key]['descriptions'].add(desc)

    consolidated = []
    for key, data in groups.items():
        qty = sum(int(p['Qty']) for p in data['parts'])
        refs = ",".join(sorted(p['Ref'] for p in data['parts']))
        
        print(f"Processing group {key} (Qty: {qty})... ", end="", flush=True)

        best_match = None
        
        # Strategy:
        # 1. If there's an existing Manufacturer_Part_Number, check if it's consistent and valid
        candidate_mpn = None
        if len(data['mfr_parts']) == 1:
            candidate_mpn = list(data['mfr_parts'])[0]
        
        if candidate_mpn:
            res = call_mouser_part_search(candidate_mpn)
            if res and res.get("SearchResults") and res["SearchResults"].get("Parts"):
                p_found = res["SearchResults"]["Parts"][0]
                p_v = parse_voltage(p_found.get("Description", ""))
                # If it meets voltage, use it
                if p_v >= data['max_v'] or not data['max_v']:
                    best_match = p_found
                    print(f"Using existing MPN: {candidate_mpn}")

        if not best_match:
            # Need to search
            search_query = ""
            if "STM32H753VIT" in key:
                search_query = "STM32H753VIT6" # Specific MPN for LQFP100
            elif "STM32C011" in key:
                search_query = "STM32C011F4U6TR" # Specific MPN for QFN20
            elif "TYPE-C-31-M-12" in key:
                search_query = "USB C Receptacle 16 pin SMT"
            elif key.startswith('RES'):
                search_query = f"Resistor thin film 1% {data['val']} {data['fp']} SMD"
            elif key.startswith('CAP'):
                ctype = data['ctype'] or ("NP0" if "pF" in data['val'] else "X7R")
                v = data['max_v'] or 50.0
                search_query = f"Capacitor {ctype} {data['val']} {v}V {data['fp']} SMD ceramic"
            else:
                mfr_part = list(data['mfr_parts'])[0] if data['mfr_parts'] else ""
                mfr_name = list(data['mfr_names'])[0] if data['mfr_names'] else ""
                val = data['val'] if data['val'] and data['val'] != mfr_part else ""
                search_query = f"{mfr_part} {mfr_name} {val}".strip()
            
            print(f"Searching: {search_query}... ", end="", flush=True)
            result = call_mouser_search(search_query)
            if result and result.get("SearchResults") and result["SearchResults"].get("Parts"):
                parts_found = result["SearchResults"]["Parts"]
                
                scored_parts = []
                for pf in parts_found:
                    score = 0
                    desc = pf.get("Description", "").upper()
                    attr_list = pf.get("ProductAttributes", [])
                    attrs = " ".join([a.get("Value", "") for a in attr_list]).upper()
                    
                    # Footprint matching
                    fp_clean = data['fp'].upper()
                    if fp_clean in desc or fp_clean in attrs:
                        score += 100
                    
                    # Package type heuristics
                    if "QFN" in fp_clean:
                        if "QFN" in desc or "MLF" in desc or "VQFPN" in desc or "UFQFPN" in desc:
                            score += 80
                        if "TSSOP" in desc or "SOIC" in desc:
                            score -= 200
                    elif "TSSOP" in fp_clean:
                        if "TSSOP" in desc:
                            score += 80
                        if "QFN" in desc:
                            score -= 200
                    
                    if key.startswith('CAP'):
                        p_v = parse_voltage(desc)
                        if p_v < data['max_v'] and data['max_v'] > 0:
                            score -= 500
                        if "SMD" in desc or "SMT" in desc:
                            score += 50
                        if "LEADED" in desc or "THROUGH HOLE" in desc:
                            score -= 1000

                    if key.startswith('RES'):
                        if "THIN FILM" in desc:
                            score += 50

                    scored_parts.append((score, pf))
                
                scored_parts.sort(key=lambda x: (x[0], -get_price(x[1], qty)), reverse=True)
                
                if scored_parts and scored_parts[0][0] > -500:
                    best_match = scored_parts[0][1]
                    print(f"Found (Score: {scored_parts[0][0]}).")
                else:
                    print("No high-quality match found.")

        unit_price = get_price(best_match, qty) if best_match else 0.0
        total_price = unit_price * qty if unit_price < 999999.0 else 0.0

        consolidated.append({
            "Refs": refs,
            "Qty": qty,
            "Value": data['val'],
            "Footprint": data['fp'],
            "Voltage": data['max_v'],
            "Mfr": best_match.get("Manufacturer", "") if best_match else (list(data['mfr_names'])[0] if data['mfr_names'] else ""),
            "MPN": best_match.get("ManufacturerPartNumber", "") if best_match else (list(data['mfr_parts'])[0] if data['mfr_parts'] else ""),
            "Mouser_PN": best_match.get("MouserPartNumber", "") if best_match else "",
            "Description": best_match.get("Description", "") if best_match else (list(data['descriptions'])[0] if data['descriptions'] else ""),
            "Unit_Price": unit_price if unit_price < 999999.0 else 0.0,
            "Total_Price": total_price
        })
        time.sleep(0.5)

    with open("hw/production/consolidated_bom.csv", "w", newline="", encoding="utf-8") as f:
        fields = ["Refs", "Qty", "Value", "Footprint", "Voltage", "Mfr", "MPN", "Mouser_PN", "Description", "Unit_Price", "Total_Price"]
        writer = csv.DictWriter(f, fieldnames=fields, quoting=csv.QUOTE_NONNUMERIC)
        writer.writeheader()
        writer.writerows(consolidated)
    
    print(f"BOM Consolidation complete. Saved to hw/production/consolidated_bom.csv")

if __name__ == "__main__":
    consolidate_bom()
