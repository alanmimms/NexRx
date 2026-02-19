
import json
import os

# Paths relative to project root
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATABASE_FILE = os.path.join(PROJECT_ROOT, "production/bom_database.json")

# Master Mapping: (Value, Footprint) -> (LCSC Part #, Unit Cost)
# Selected for JLCPCB Basic/Extended library availability
lcsc_mapping = {
    # === Resistors 0402 1% ===
    ("10k", "Resistor_SMD:R_0402_1005Metric"): ("C25744", 0.001),
    ("1k", "Resistor_SMD:R_0402_1005Metric"): ("C21190", 0.001),
    ("100", "Resistor_SMD:R_0402_1005Metric"): ("C25076", 0.001),
    ("33", "Resistor_SMD:R_0402_1005Metric"): ("C25092", 0.001),
    ("0", "Resistor_SMD:R_0402_1005Metric"): ("C17168", 0.001),
    ("47k", "Resistor_SMD:R_0402_1005Metric"): ("C25790", 0.001),
    ("49.9", "Resistor_SMD:R_0402_1005Metric"): ("C25119", 0.001),
    ("22", "Resistor_SMD:R_0402_1005Metric"): ("C25083", 0.001),
    ("100k", "Resistor_SMD:R_0402_1005Metric"): ("C25741", 0.001),
    ("200k", "Resistor_SMD:R_0402_1005Metric"): ("C25753", 0.001),
    ("22.1k", "Resistor_SMD:R_0402_1005Metric"): ("C25084", 0.001),
    ("8.06k", "Resistor_SMD:R_0402_1005Metric"): ("C25109", 0.001),
    
    # === Resistors 0603 1% ===
    ("10k", "Resistor_SMD:R_0603_1608Metric"): ("C25804", 0.002),
    ("1k", "Resistor_SMD:R_0603_1608Metric"): ("C21189", 0.002),
    ("33", "Resistor_SMD:R_0603_1608Metric"): ("C25105", 0.002),
    ("330", "Resistor_SMD:R_0603_1608Metric"): ("C23138", 0.002),
    ("0", "Resistor_SMD:R_0603_1608Metric"): ("C21188", 0.002),
    ("49.9", "Resistor_SMD:R_0603_1608Metric"): ("C22790", 0.002),
    ("12.1k", "Resistor_SMD:R_0603_1608Metric"): ("C22765", 0.002),
    ("22.1k", "Resistor_SMD:R_0603_1608Metric"): ("C22809", 0.002),
    ("5.1k", "Resistor_SMD:R_0603_1608Metric"): ("C23182", 0.002),
    ("562", "Resistor_SMD:R_0603_1608Metric"): ("C23204", 0.002),
    ("85k", "Resistor_SMD:R_0603_1608Metric"): ("C23229", 0.002),
    ("100k", "Resistor_SMD:R_0603_1608Metric"): ("C25803", 0.002),
    ("34", "Resistor_SMD:R_0603_1608Metric"): ("C22835", 0.002),
    
    # === Resistors 0805 ===
    ("270", "Resistor_SMD:R_0805_2012Metric"): ("C17557", 0.003),

    # === Capacitors 0603 (Standard) ===
    ("100nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14663", 0.005),
    ("1uF", "Capacitor_SMD:C_0603_1608Metric"): ("C15849", 0.008),
    ("10uF", "Capacitor_SMD:C_0603_1608Metric"): ("C19702", 0.015),
    ("10nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14664", 0.005),
    ("4.7uF", "Capacitor_SMD:C_0603_1608Metric"): ("C19666", 0.012),
    
    # === Capacitors 0805 (Bulk) ===
    ("2.2uF", "Capacitor_SMD:C_0805_2012Metric"): ("C16133", 0.15),
    ("4.7uF", "Capacitor_SMD:C_0805_2012Metric"): ("C7192", 0.18),
    ("10uF", "Capacitor_SMD:C_0805_2012Metric"): ("C45112", 0.02),
    
    # === Capacitors 1206 ===
    ("100nF", "Capacitor_SMD:C_1206_3216Metric"): ("C14662", 0.015),
    ("100uF", "Capacitor_SMD:C_1206_3216Metric"): ("C45545", 0.35),

    # === NP0/C0G for RF (0603) ===
    ("470pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14691", 0.006),
    ("1nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14665", 0.008),
    ("120pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14682", 0.006),
    ("15pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14673", 0.006),
    ("33pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14678", 0.006),
    ("68pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14680", 0.006),
    ("250pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14687", 0.006),
    ("150pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14684", 0.006),
    ("220pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14686", 0.006),
    ("3.3nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14668", 0.008),
    ("8pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14670", 0.006),
    ("3.9nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14669", 0.008),
    ("560pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14692", 0.006),
    ("8.2nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14667", 0.008),
    ("2.2nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14666", 0.008),
    ("8.2pF", "Capacitor_SMD:C_0402_1005Metric"): ("C1571", 0.005),
    ("1uF", "Capacitor_SMD:C_0402_1005Metric"): ("C52923", 0.008),
    
    # === Mechanical / Placeholder ===
    ("MountingHole", "MountingHole:MountingHole_3.2mm_M3_ISO7380_Pad"): ("DNP", 0.0),
    ("PA0", "TestPoint:TestPoint_Pad_1.0x1.0mm"): ("DNP", 0.0),
    
    # === Magnetics / Hand-Wound ===
    ("INPUT", "Library:Transformer-unun-BN-43-202"): ("MANUAL_WIND", 0.0),
    ("BN-43-202, 200 : 3-way 22 ohm, hexafilar 2 turns #30 AWG", "Library:Transformer-hexafilar-200-to-3x22-ohm-BN-43-202"): ("MANUAL_WIND", 0.0),
}

with open(DATABASE_FILE, 'r') as f:
    db = json.load(f)

count = 0
for uuid, data in db.items():
    key = (data['value'], data['footprint'])
    
    if key in lcsc_mapping:
        mapping = lcsc_mapping[key]
        if not data.get("LCSC_PN"):
            db[uuid]["LCSC_PN"] = mapping[0]
            db[uuid]["LCSC_Cost"] = mapping[1]
            count += 1
    
    # Generic fallback
    if not db[uuid].get("LCSC_PN") and not db[uuid].get("Mouser_PN"):
        if "Resistor" in data['footprint']:
            db[uuid]["cost"] = 0.005
            db[uuid]["Source"] = "Generic 0.1%"
        elif "Capacitor" in data['footprint']:
            db[uuid]["cost"] = 0.01
            db[uuid]["Source"] = "Generic MLCC"
        elif not db[uuid].get("LCSC_Cost") and not db[uuid].get("Mouser_Cost") and not db[uuid].get("cost"):
            db[uuid]["cost"] = 1.00
            db[uuid]["Source"] = "IC Est."

with open(DATABASE_FILE, 'w') as f:
    json.dump(db, f, indent=2)

print(f"Enriched {count} components with LCSC numbers.")
