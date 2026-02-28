import csv
import re
import os
import subprocess

def simplify_footprint(fp):
    # Extract "0402", "0603", "0805", "1206", "1210" etc.
    m = re.search(r'(0201|0402|0603|0805|1206|1210)', fp)
    if m:
        return m.group(1)
    parts = fp.split(':')
    return parts[-1] if len(parts) > 1 else fp

def get_cap_desc(val, fp, ctype, volt):
    simp_fp = simplify_footprint(fp)
    ctype_lower = str(ctype).lower()
    fp_lower = fp.lower()
    
    cap_type = "MLCC"
    if "tant" in ctype_lower or "tantalum" in fp_lower:
        cap_type = "Tantalum"
    elif "elec" in ctype_lower or "electrolytic" in fp_lower:
        cap_type = "Electrolytic"

    dielectric = ctype if ctype and ctype.lower() not in ["tant", "tantalum"] else ""
    if not dielectric and cap_type == "MLCC":
        if "pF" in val or val in ["1nF", "2.2nF", "3.3nF", "3.9nF", "4.7nF"]:
            dielectric = "NP0"
        elif "uF" in val and "0402" in simp_fp:
            dielectric = "X5R"
        else:
            dielectric = "X7R"

    voltage = volt if volt else "50V"
    tol = "10%" if (cap_type == "Tantalum" or "uF" in val) else "5%"
    if dielectric.upper() == "NP0": tol = "5%"

    parts = ["Capacitor", cap_type]
    if dielectric: parts.append(dielectric)
    parts.extend([voltage, tol, val, simp_fp])
    return ", ".join(parts)

def get_res_desc(val, fp):
    simp_fp = simplify_footprint(fp)
    return f"Resistor, Thin Film, 0.1%, {val}, {simp_fp}"

def process_bom():
    # 1. Export from KiCad
    fields = [
        "Reference", "Value", "Footprint", "Description",
        "Manufacturer", "Manufacturer_Name", "MF", "Mfg", "Mfr", "Vendor",
        "Manufacturer_Part_Number", "MP", "MPN", "MfgPart",
        "LCSC", "LCSC_PN", "JLCPCBpn",
        "Mouser_PN", "Mouser Part Number",
        "cap-type", "working-voltage", "${QUANTITY}"
    ]
    labels = [
        "Refs", "Value", "Footprint", "Description",
        "M1", "M2", "M3", "M4", "M5", "M6",
        "P1", "P2", "P3", "P4",
        "L1", "L2", "L3",
        "MO1", "MO2",
        "CapType", "Voltage", "Qty"
    ]
    
    cmd = [
        "kicad-cli", "sch", "export", "bom",
        "--fields", ",".join(fields),
        "--labels", ",".join(labels),
        "--group-by", "Value,Footprint",
        "--output", "production/temp-bom.csv",
        "hw/NexRx.kicad_sch"
    ]
    
    subprocess.run(cmd, check=True)

    # 2. Process
    with open("production/temp-bom.csv", "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    def first_val(s):
        if not s: return ""
        # Grouped rows might have "val1,val2" or "val1"
        parts = [p.strip() for p in s.split(',')]
        return next((p for p in parts if p), "")

    unified_rows = []
    for r in rows:
        refs = r['Refs']
        val = r['Value']
        fp = r['Footprint']
        
        # Collapse Manufacturer
        mfr = next((first_val(r.get(k)) for k in ["M1", "M2", "M3", "M4", "M5", "M6"] if first_val(r.get(k))), "")
        # Collapse MPN
        mpn = next((first_val(r.get(k)) for k in ["P1", "P2", "P3", "P4"] if first_val(r.get(k))), "")
        # Collapse LCSC
        lcsc = next((first_val(r.get(k)) for k in ["L1", "L2", "L3"] if first_val(r.get(k))), "")
        # Collapse Mouser
        mouser = next((first_val(r.get(k)) for k in ["MO1", "MO2"] if first_val(r.get(k))), "")

        # Update Description
        desc = r['Description']
        if refs.startswith('C'):
            desc = get_cap_desc(val, fp, first_val(r['CapType']), first_val(r['Voltage']))
        elif refs.startswith('R'):
            desc = get_res_desc(val, fp)

        unified_rows.append({
            'Refs': refs,
            'Qty': r['Qty'],
            'Value': val,
            'Footprint': simplify_footprint(fp),
            'Manufacturer': mfr,
            'MPN': mpn,
            'LCSC_PN': lcsc,
            'Mouser_PN': mouser,
            'Description': desc
        })

    # 3. Save Unified BOM
    out_fields = ['Refs', 'Qty', 'Value', 'Footprint', 'Manufacturer', 'MPN', 'LCSC_PN', 'Mouser_PN', 'Description']
    with open("production/nexrx-bom.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=out_fields)
        writer.writeheader()
        writer.writerows(unified_rows)

    print(f"Generated unified BOM with {len(unified_rows)} rows.")
    return unified_rows

def update_schematics(unified_rows):
    # This function would back-propagate properties to .kicad_sch files.
    # It requires parsing the s-expr and finding symbols by UUID or Reference.
    # For now, let's just confirm the BOM is correct.
    pass

if __name__ == "__main__":
    process_bom()
