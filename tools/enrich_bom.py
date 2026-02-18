
import json
import os

# Paths relative to project root
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATABASE_FILE = os.path.join(PROJECT_ROOT, "production/bom_database.json")

# Master Mapping: (Value, Footprint) -> Dictionary of Metadata
master_mapping = {
    # === Resistors 0402 ===
    ("10k", "Resistor_SMD:R_0402_1005Metric"): {
        "LCSC_PN": "C25744", "LCSC_Cost": 0.001,
        "Mouser_PN": "71-CRCW040210K0FKED", "Mouser_Cost": 0.01
    },
    ("1k", "Resistor_SMD:R_0402_1005Metric"): {
        "LCSC_PN": "C21190", "LCSC_Cost": 0.001,
        "Mouser_PN": "71-CRCW04021K00FKED", "Mouser_Cost": 0.01
    },
    
    # === Capacitors 0603 (RF / NP0) ===
    ("470pF", "Capacitor_SMD:C_0603_1608Metric"): {
        "LCSC_PN": "C14691", "LCSC_Cost": 0.006,
        "Mouser_PN": "810-C1608C0G1H471J", "Mouser_Cost": 0.05
    },
    
    # === Critical ICs ===
    ("STM32H753VITx", "Package_QFP:LQFP-100_14x14mm_P0.5mm"): {
        "MF": "STMicroelectronics", "MP": "STM32H753VIT6",
        "LCSC_PN": "C191568", "LCSC_Cost": 15.50,
        "Mouser_PN": "497-STM32H753VIT6", "Mouser_Cost": 18.20
    },
    ("AK5578EN", "QFN50P900X900X100-65N"): {
        "MF": "Asahi Kasei", "MP": "AK5578EN",
        "Mouser_PN": "412-AK5578EN", "Mouser_Cost": 18.50,
        "LCSC_PN": "GLOBAL_SOURCE", "LCSC_Cost": 18.00
    },
}

# Example helper to populate common 0603 values if missing
def get_generic_passive(val, footprint):
    if "Resistor" in footprint:
        return {"cost": 0.002, "Source": "Generic"}
    if "Capacitor" in footprint:
        return {"cost": 0.005, "Source": "Generic"}
    return {}

with open(DATABASE_FILE, 'r') as f:
    db = json.load(f)

for uuid, data in db.items():
    key = (data['value'], data['footprint'])
    
    # 1. Apply Master Mapping (Highest Priority)
    if key in master_mapping:
        db[uuid].update(master_mapping[key])
    else:
        # 2. Apply Generic logic for unsourced parts
        if "Resistor" in data['footprint']:
            db[uuid]["cost"] = 0.005
            db[uuid]["Source"] = "Generic 0.1%"
        elif "Capacitor" in data['footprint']:
            db[uuid]["cost"] = 0.01
            db[uuid]["Source"] = "Generic MLCC"
        elif not db[uuid].get("cost"):
            # IC fallback
            db[uuid]["cost"] = 1.00
            db[uuid]["Source"] = "IC Est."

with open(DATABASE_FILE, 'w') as f:
    json.dump(db, f, indent=2)

print("Database Enriched with Multi-Source data.")
