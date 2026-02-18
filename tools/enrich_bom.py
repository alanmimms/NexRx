
import json
import os

DATABASE_FILE = "production/bom_database.json"

# LCSC Mapping Table (Value, Footprint) -> LCSC Part #
# Focus on common JLCPCB Basic/Extended parts for cost efficiency
lcsc_mapping = {
    # Resistors 0402 1%
    ("10k", "Resistor_SMD:R_0402_1005Metric"): "C25744",
    ("1k", "Resistor_SMD:R_0402_1005Metric"): "C21190",
    ("100", "Resistor_SMD:R_0402_1005Metric"): "C25076",
    ("33", "Resistor_SMD:R_0402_1005Metric"): "C25092",
    ("0", "Resistor_SMD:R_0402_1005Metric"): "C17168",
    ("47k", "Resistor_SMD:R_0402_1005Metric"): "C25790",
    ("49.9", "Resistor_SMD:R_0402_1005Metric"): "C25119",
    ("22", "Resistor_SMD:R_0402_1005Metric"): "C25083",
    
    # Capacitors 0402 X7R/X5R
    ("100nF", "Capacitor_SMD:C_0402_1005Metric"): "C1525",
    ("1uF", "Capacitor_SMD:C_0402_1005Metric"): "C52923",
    ("10uF", "Capacitor_SMD:C_0603_1608Metric"): "C19702",
    ("10nF", "Capacitor_SMD:C_0402_1005Metric"): "C1519",
    ("4.7uF", "Capacitor_SMD:C_0603_1608Metric"): "C19666",
    
    # NP0/C0G for RF (Critical)
    ("470pF", "Capacitor_SMD:C_0402_1005Metric"): "C1550", # NP0 50V
    ("1nF", "Capacitor_SMD:C_0402_1005Metric"): "C1523",   # NP0 50V
    ("120pF", "Capacitor_SMD:C_0402_1005Metric"): "C1541",
    ("15pF", "Capacitor_SMD:C_0402_1005Metric"): "C1548",
    ("33pF", "Capacitor_SMD:C_0402_1005Metric"): "C1554",
    ("68pF", "Capacitor_SMD:C_0402_1005Metric"): "C1564",
    ("250pF", "Capacitor_SMD:C_0402_1005Metric"): "C1546",
    ("560pF", "Capacitor_SMD:C_1552"): "C1552", # Fixed 560pF 0402 NP0
    
    # Specialized ICs
    ("SN74LVC2G14DBV", "Package_TO_SOT_SMD:SOT-23-6"): ("C131122", 0.15),
    ("MCP23S17", "Package_DFN_QFN:QFN-28-1EP_6x6mm_P0.65mm_EP4.25x4.25mm"): ("C60341", 1.20),
    ("TPS22918DBVR", "Package_TO_SOT_SMD:SOT-23-6"): ("C157838", 0.45),
    ("USB3343", "Package_DFN_QFN:QFN-24-1EP_4x4mm_P0.5mm_EP2.6x2.6mm"): ("C11558", 1.80),
    ("STM32H753VITx", "Package_QFP:LQFP-100_14x14mm_P0.5mm"): ("C191568", 15.50),
    ("ICE40UP5K-SG48I", "Package_DFN_QFN:QFN-48-1EP_7x7mm_P0.5mm_EP5.6x5.6mm"): ("C155631", 9.80),
    ("AK5578EN", "QFN50P900X900X100-65N"): ("GLOBAL_SOURCE", 18.00),
    ("AS183-92LF", "Library:SOT65P220X110-6N"): ("C152415", 0.85),
    ("TS3A4751RUCR", "Library:TS3A4751-RUC14"): ("C128911", 1.10),
}

with open(DATABASE_FILE, 'r') as f:
    db = json.load(f)

count = 0
for uuid, data in db.items():
    key = (data['value'], data['footprint'])
    
    # Generic passives cost estimation
    if not data.get("cost"):
        if "Resistor" in data['footprint']: db[uuid]["cost"] = 0.001
        elif "Capacitor" in data['footprint']: db[uuid]["cost"] = 0.005
        
    if key in lcsc_mapping:
        mapping = lcsc_mapping[key]
        if isinstance(mapping, tuple):
            db[uuid]["LCSC"] = mapping[0]
            db[uuid]["cost"] = mapping[1]
        else:
            db[uuid]["LCSC"] = mapping
        count += 1

with open(DATABASE_FILE, 'w') as f:
    json.dump(db, f, indent=2)

print(f"Enriched {count} components with LCSC part numbers.")
