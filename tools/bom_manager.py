import re
import os
import glob
import json
import csv

# Paths relative to project root
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATABASE_FILE = os.path.join(PROJECT_ROOT, "production/bom_database.json")
CSV_FILE = os.path.join(PROJECT_ROOT, "production/nexrx_bom.csv")
PROJECTION_FILE = os.path.join(PROJECT_ROOT, "production/cost_projection.csv")
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

def calculate_tier_cost(unit_cost, target_volume):
    if target_volume <= 1:
        return unit_cost
    # Industry standard price break estimates
    if unit_cost < 0.10: # Passives
        if target_volume >= 10000: return unit_cost * 0.70 # 30% off
        if target_volume >= 1000: return unit_cost * 0.85 # 15% off
        return unit_cost * 0.95 # 5% off
    else: # ICs and specialized parts
        if target_volume >= 10000: return unit_cost * 0.50 # 50% off
        if target_volume >= 1000: return unit_cost * 0.65 # 35% off
        return unit_cost * 0.85 # 15% off

def extract_metadata():
    db = load_database()
    # Match symbols robustly
    symbol_pattern = re.compile(r'\(symbol\s+\(lib_id\s+"[^"]+"\).*?\(uuid\s+"([^"]+)"\)\s+\)', re.DOTALL)
    
    current_uuids = set()
    
    for sch_file in glob.glob(SCH_GLOB):
        with open(sch_file, 'r') as f:
            content = f.read()
            
        matches = symbol_pattern.finditer(content)
        for m in matches:
            block = m.group(0)
            uuid = m.group(1)
            current_uuids.add(uuid)
            
            def get_prop(name):
                p = re.search(r'\(property\s+"' + name + r'"\s+"([^"]*)"', block)
                return p.group(1) if p else ""

            ref = get_prop("Reference")
            if ref.startswith('#') or ref.startswith('GND') or ref.startswith('FID'):
                continue
                
            val = get_prop("Value")
            foot = get_prop("Footprint")
            
            mp_sch = get_prop("MP") or get_prop("Manufacturer_Part_Number") or get_prop("Part Number") or get_prop("MfgPart") or get_prop("MPN") or ""
            mf_sch = get_prop("MF") or get_prop("Manufacturer") or get_prop("Manufacturer_Name") or get_prop("Mfg") or ""
            lcsc_sch = get_prop("LCSC") or get_prop("JLCPCBpn") or ""
            
            if uuid not in db:
                db[uuid] = {}
            
            db[uuid].update({
                "ref": ref,
                "value": val,
                "footprint": foot,
                "file": os.path.basename(sch_file)
            })
            
            if mp_sch and not db[uuid].get("MP"): db[uuid]["MP"] = mp_sch
            if mf_sch and not db[uuid].get("MF"): db[uuid]["MF"] = mf_sch
            if lcsc_sch and not db[uuid].get("LCSC_PN"): db[uuid]["LCSC_PN"] = lcsc_sch
            
            ct = get_prop("cap-type")
            wv = get_prop("working-voltage")
            if ct: db[uuid]["cap-type"] = ct
            if wv: db[uuid]["working-voltage"] = wv

    # PRUNE orphaned UUIDs
    orphans = [u for u in db if u not in current_uuids]
    for u in orphans:
        del db[u]
    if orphans:
        print(f"Pruned {len(orphans)} orphaned components from database.")

    save_database(db)
    return db

def export_csv(db):
    bom = {}
    for uuid, data in db.items():
        key = data.get("MP") or f"{data['value']}_{data['footprint']}"
        if key not in bom:
            bom[key] = {
                "Qty": 0, "Value": data['value'], "MF": data.get("MF", ""), "MP": data.get("MP", ""),
                "LCSC_PN": data.get("LCSC_PN", ""), "Mouser_PN": data.get("Mouser_PN", ""),
                "DigiKey_PN": data.get("DigiKey_PN", ""), "Refs": [], "Footprint": data['footprint'],
                "CapType": data.get("cap-type", ""), "Voltage": data.get("working-voltage", ""),
                "UnitCost": 0.0, "Source": "Manual"
            }
        bom[key]["Qty"] += 1
        bom[key]["Refs"].append(data['ref'])
        if data.get("LCSC_Cost"):
            bom[key]["UnitCost"], bom[key]["Source"] = data["LCSC_Cost"], "LCSC"
        elif data.get("Mouser_Cost"):
            bom[key]["UnitCost"], bom[key]["Source"] = data["Mouser_Cost"], "Mouser"
        elif data.get("Source"):
            bom[key]["UnitCost"], bom[key]["Source"] = data.get("cost", 0.0), data["Source"]
        elif data.get("cost"):
            bom[key]["UnitCost"], bom[key]["Source"] = data["cost"], "Manual"

    total_project_cost = 0.0
    with open(CSV_FILE, "w", newline="") as f:
        writer = csv.writer(f, quoting=csv.QUOTE_NONNUMERIC)
        writer.writerow(["Qty", "Value", "Manufacturer", "Part Number", "LCSC PN", "Mouser PN", "DigiKey PN", "Designators", "Footprint", "Cap Type", "Voltage", "Unit Cost", "Source", "Subtotal"])
        for key in sorted(bom.keys()):
            b = bom[key]
            subtotal = b["Qty"] * b["UnitCost"]
            total_project_cost += subtotal
            writer.writerow([b["Qty"], b["Value"], b["MF"], b["MP"], b["LCSC_PN"], b["Mouser_PN"], b["DigiKey_PN"], ", ".join(sorted(b["Refs"])), b["Footprint"], b["CapType"], b["Voltage"], f"${b['UnitCost']:.4f}", b["Source"], f"${subtotal:.2f}"])
        writer.writerow(["", "", "", "", "", "", "", "", "", "", "", "TOTAL", "", f"${total_project_cost:.2f}"])

    tiers = [1, 100, 1000, 10000]
    with open(PROJECTION_FILE, "w", newline="") as f:
        writer = csv.writer(f, quoting=csv.QUOTE_NONNUMERIC)
        writer.writerow(["Volume Tier", "Project Unit Cost", "Total Production Cost", "Savings vs Qty 1"])
        for t in tiers:
            tier_total = sum(b["Qty"] * calculate_tier_cost(b["UnitCost"], t) for b in bom.values())
            savings = 0 if t == 1 else (1 - (tier_total / total_project_cost)) * 100
            writer.writerow([f"Qty {t}", f"${tier_total:.2f}", f"${tier_total * t:,.2f}", f"{savings:.1f}%"])
    print(f"BOM and Projection exported to production/")

if __name__ == "__main__":
    db = extract_metadata()
    export_csv(db)
