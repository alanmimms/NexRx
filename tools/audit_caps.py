
import re
import os
import glob

def audit_caps():
    caps = []
    symbol_pattern = re.compile(r'(\(symbol\s+\(lib_id\s+"[^"]+"\)\s+\(at\s+[\d.-]+\s+[\d.-]+\s+\d+\).*?\(uuid\s+"[^"]+"\)\s+\))', re.DOTALL)
    
    for sch_file in glob.glob("hw/*.kicad_sch"):
        with open(sch_file, 'r') as f:
            content = f.read()
            
        symbol_instances = symbol_pattern.findall(content)
        
        for block in symbol_instances:
            def get_prop(name):
                m = re.search(r'\(property\s+"' + name + r'"\s+"([^"]*)"', block)
                return m.group(1) if m else ""

            ref = get_prop("Reference")
            val = get_prop("Value")
            if not ref.startswith('C'): continue
            
            cap_type = get_prop("cap-type")
            voltage = get_prop("working-voltage")
            footprint = get_prop("Footprint")
            
            caps.append({
                "Ref": ref,
                "Value": val,
                "Type": cap_type,
                "Voltage": voltage,
                "Footprint": footprint,
                "File": os.path.basename(sch_file)
            })
    return caps

all_caps = audit_caps()
print(f"{'Ref':<10} | {'Value':<10} | {'Type':<10} | {'Volt':<10} | {'File'}")
print("-" * 60)
for c in sorted(all_caps, key=lambda x: x['Ref']):
    print(f"{c['Ref']:<10} | {c['Value']:<10} | {c['Type']:<10} | {c['Voltage']:<10} | {c['File']}")
