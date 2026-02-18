
import re
import os
import glob
import json
import csv

# Paths relative to project root
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATABASE_FILE = os.path.join(PROJECT_ROOT, "production/bom_database.json")
CSV_FILE = os.path.join(PROJECT_ROOT, "production/nexrx_bom.csv")
SCH_GLOB = os.path.join(PROJECT_ROOT, "hw/*.kicad_sch")

def load_database():
    if os.path.exists(DATABASE_FILE):
        with open(DATABASE_FILE, 'r') as f:
            return json.load(f)
    return {}

def save_database(db):
    os.makedirs(os.path.dirname(DATABASE_FILE), exist_ok=True)
    with open(DATABASE_FILE, 'w') as f:
        json.dump(db, f, indent=2)

def extract_metadata():
    db = load_database()
    # Match symbols robustly
    symbol_pattern = re.compile(r'\(symbol\s+\(lib_id\s+"[^"]+"\).*?\(uuid\s+"([^"]+)"\)\s+\)', re.DOTALL)
    
    for sch_file in glob.glob(SCH_GLOB):
        with open(sch_file, 'r') as f:
            content = f.read()
            
        matches = symbol_pattern.finditer(content)
        for m in matches:
            block = m.group(0)
            uuid = m.group(1)
            
            def get_prop(name):
                # property "Name" "Value"
                # property "Name" "Value" (at ...)
                p = re.search(r'\(property\s+"' + name + r'"\s+"([^"]*)"', block)
                return p.group(1) if p else ""

            ref = get_prop("Reference")
            if ref.startswith('#') or ref.startswith('GND') or ref.startswith('FID'):
                continue
                
            val = get_prop("Value")
            foot = get_prop("Footprint")
            
            # Extract possible MPN fields from schematic
            mp_sch = get_prop("MP") or get_prop("Manufacturer_Part_Number") or get_prop("Part Number") or get_prop("MfgPart") or get_prop("MPN") or ""
            mf_sch = get_prop("MF") or get_prop("Manufacturer") or get_prop("Manufacturer_Name") or get_prop("Mfg") or ""
            lcsc_sch = get_prop("LCSC") or get_prop("JLCPCBpn") or ""
            
            # Initial database entry if new
            if uuid not in db:
                db[uuid] = {}
            
            # Update basic tracking info
            db[uuid].update({
                "ref": ref,
                "value": val,
                "footprint": foot,
                "file": os.path.basename(sch_file)
            })
            
            # PRESERVE existing database metadata if present, otherwise take from schematic
            if mp_sch and not db[uuid].get("MP"): db[uuid]["MP"] = mp_sch
            if mf_sch and not db[uuid].get("MF"): db[uuid]["MF"] = mf_sch
            if lcsc_sch and not db[uuid].get("LCSC"): db[uuid]["LCSC"] = lcsc_sch
            
            # Capacitor specifics
            ct = get_prop("cap-type")
            wv = get_prop("working-voltage")
            if ct: db[uuid]["cap-type"] = ct
            if wv: db[uuid]["working-voltage"] = wv

    save_database(db)
    return db

def export_csv(db):
    # Group by MP or (Value, Footprint) if MP missing
    bom = {}
    for uuid, data in db.items():
        key = data.get("MP") or f"{data['value']}_{data['footprint']}"
        if key not in bom:
            bom[key] = {
                "Qty": 0,
                "Value": data['value'],
                "MF": data.get("MF", ""),
                "MP": data.get("MP", ""),
                "LCSC": data.get("LCSC", ""),
                "Refs": [],
                "Footprint": data['footprint'],
                "CapType": data.get("cap-type", ""),
                "Voltage": data.get("working-voltage", ""),
                "UnitCost": data.get("cost", 0.0)
            }
        bom[key]["Qty"] += 1
        bom[key]["Refs"].append(data['ref'])

    total_project_cost = 0.0
    with open(CSV_FILE, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Qty", "Value", "Manufacturer", "Part Number", "LCSC", "Designators", "Footprint", "Cap Type", "Voltage", "Unit Cost", "Subtotal"])
        for key in sorted(bom.keys()):
            b = bom[key]
            subtotal = b["Qty"] * b["UnitCost"]
            total_project_cost += subtotal
            writer.writerow([b["Qty"], b["Value"], b["MF"], b["MP"], b["LCSC"], ", ".join(sorted(b["Refs"])), b["Footprint"], b["CapType"], b["Voltage"], f"${b['UnitCost']:.4f}", f"${subtotal:.2f}"])
        
        writer.writerow([])
        writer.writerow(["", "", "", "", "", "", "", "", "TOTAL ESTIMATED COST", "", f"${total_project_cost:.2f}"])

db = extract_metadata()
export_csv(db)
print(f"BOM Database updated with {len(db)} unique component UUIDs.")
print("BOM exported to nexrx_bom.csv")
