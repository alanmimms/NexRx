
import re

def analyze_rf200(pcb_path):
    nets = {}
    rf200_nets = []
    rf200_nums = []
    zones = []
    segments = []
    
    current_data = ""
    depth = 0
    in_block = False
    
    with open(pcb_path, 'r') as f:
        for line in f:
            if not in_block:
                if '(net ' in line and ')' in line:
                    m = re.search(r'\(net (\d+) "([^"]+)"\)', line)
                    if m:
                        n_num, n_name = int(m.group(1)), m.group(2)
                        nets[n_num] = n_name
                        if 'rf' in n_name.lower() or '200' in n_name.lower():
                            if 'enable' not in n_name.lower() and 'ctrl' not in n_name.lower():
                                rf200_nets.append(n_name)
                                rf200_nums.append(n_num)
                
                if '(zone' in line or '(segment' in line:
                    in_block = True
                    current_data = line
                    depth = line.count('(') - line.count(')')
            else:
                current_data += line
                depth += line.count('(') - line.count(')')
                if depth <= 0:
                    # End of block
                    if '(zone' in current_data:
                        if 'name "rf200"' in current_data:
                            layers_m = re.search(r'\(layers (.*?)\)', current_data)
                            pts = re.findall(r'\(xy ([\-\d\.]+) ([\-\d\.]+)\)', current_data)
                            if layers_m and pts:
                                zones.append({
                                    'layers': layers_m.group(1),
                                    'pts': [(float(x), float(y)) for x, y in pts]
                                })
                    elif '(segment' in current_data:
                        net_m = re.search(r'\(net (\d+)\)', current_data)
                        if net_m:
                            num = int(net_m.group(1))
                            if num in rf200_nums:
                                start_m = re.search(r'\(start ([\-\d\.]+) ([\-\d\.]+)\)', current_data)
                                end_m = re.search(r'\(end ([\-\d\.]+) ([\-\d\.]+)\)', current_data)
                                layer_m = re.search(r'\(layer "([^"]+)"\)', current_data)
                                if start_m and end_m and layer_m:
                                    segments.append({
                                        'net': nets[num],
                                        'start': (float(start_m.group(1)), float(start_m.group(2))),
                                        'end': (float(end_m.group(1)), float(end_m.group(2))),
                                        'layer': layer_m.group(1)
                                    })
                    in_block = False
                    current_data = ""

    print(f"Candidate RF nets: {rf200_nets}")
    print(f"Found {len(zones)} 'rf200' canyon zones.")
    print(f"Found {len(segments)} segments for target nets.")
    
    for seg in segments:
        mid_x = (seg['start'][0] + seg['end'][0]) / 2
        mid_y = (seg['start'][1] + seg['end'][1]) / 2
        
        covered = False
        for z in zones:
            min_x = min(p[0] for p in z['pts'])
            max_x = max(p[0] for p in z['pts'])
            min_y = min(p[1] for p in z['pts'])
            max_y = max(p[1] for p in z['pts'])
            
            if min_x <= mid_x <= max_x and min_y <= mid_y <= max_y:
                covered = True
                break
        
        status = "OK" if covered else "MISS"
        layer_err = "" if seg['layer'] == 'F.Cu' else f" (WRONG LAYER: {seg['layer']})"
        
        if not covered or layer_err:
            print(f"{status}: {seg['net']} at ({mid_x:.2f}, {mid_y:.2f}){layer_err}")

if __name__ == "__main__":
    analyze_rf200("hw/NexRx.kicad_pcb")
