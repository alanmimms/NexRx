
import math
import re
import os

# Net categories
qsd_bb_nets = {
    "QSD0_I+": 20, "QSD0_I-": 21, "QSD0_Q+": 27, "QSD0_Q-": 29,
    "QSD1_I+": 31, "QSD1_I-": 32, "QSD1_Q+": 22, "QSD1_Q-": 23,
    "QSD2_I+": 33, "QSD2_I-": 34, "QSD2_Q+": 25, "QSD2_Q-": 26
}

qsd_clk_nets = {
    "QSD0_Ph0": 109, "QSD0_Ph1": 100, "QSD0_Ph2": 106, "QSD0_Ph3": 101,
    "QSD1_Ph0": 103, "QSD1_Ph1": 99,  "QSD1_Ph2": 98,  "QSD1_Ph3": 105,
    "QSD2_Ph0": 102, "QSD2_Ph1": 107, "QSD2_Ph2": 108, "QSD2_Ph3": 110
}

all_nets = {**qsd_bb_nets, **qsd_clk_nets}
net_lengths = {name: 0.0 for name in all_nets}
id_to_name = {id: name for name, id in all_nets.items()}

pcb_file = "hw/NexRx.kicad_pcb"
board_thickness = 1.5582

if not os.path.exists(pcb_file):
    print(f"Error: {pcb_file} not found.")
    exit(1)

with open(pcb_file, "r") as f:
    state = None
    points = {}
    current_net = None
    for line in f:
        line = line.strip()
        if line.startswith("(segment"):
            state = "segment"
            points = {}
            current_net = None
        elif line.startswith("(arc"):
            state = "arc"
            points = {}
            current_net = None
        elif line.startswith("(via"):
            state = "via"
            points = {}
            current_net = None
        elif state in ["segment", "arc", "via"]:
            if line.startswith("(at"):
                m = re.search(r"\(at ([\d.-]+) ([\d.-]+)\)", line)
                if m: points["at"] = (float(m.group(1)), float(m.group(2)))
            elif line.startswith("(start"):
                m = re.search(r"\(start ([\d.-]+) ([\d.-]+)\)", line)
                if m: points["start"] = (float(m.group(1)), float(m.group(2)))
            elif line.startswith("(end"):
                m = re.search(r"\(end ([\d.-]+) ([\d.-]+)\)", line)
                if m: points["end"] = (float(m.group(1)), float(m.group(2)))
            elif line.startswith("(mid") and state == "arc":
                m = re.search(r"\(mid ([\d.-]+) ([\d.-]+)\)", line)
                if m: points["mid"] = (float(m.group(1)), float(m.group(2)))
            elif line.startswith("(net "):
                m = re.search(r"\(net (\d+)\)", line)
                if m: current_net = int(m.group(1))
            elif line == ")":
                if current_net in id_to_name:
                    name = id_to_name[current_net]
                    if state == "segment" and "start" in points and "end" in points:
                        d = math.sqrt((points["start"][0]-points["end"][0])**2 + (points["start"][1]-points["end"][1])**2)
                        net_lengths[name] += d
                    elif state == "via":
                        net_lengths[name] += board_thickness
                    elif state == "arc" and "start" in points and "mid" in points and "end" in points:
                        A, B, C = points["start"], points["mid"], points["end"]
                        def get_center(P1, P2, P3):
                            x1, y1 = P1; x2, y2 = P2; x3, y3 = P3
                            D = 2 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2))
                            if abs(D) < 1e-9: return None
                            ux = ((x1**2 + y1**2) * (y2 - y3) + (x2**2 + y2**2) * (y3 - y1) + (x3**2 + y3**2) * (y1 - y2)) / D
                            uy = ((x1**2 + y1**2) * (x3 - x2) + (x2**2 + y2**2) * (x1 - x3) + (x3**2 + y3**2) * (x2 - x1)) / D
                            return (ux, uy)
                        center = get_center(A, B, C)
                        if center:
                            r = math.sqrt((A[0]-center[0])**2 + (A[1]-center[1])**2)
                            ang1 = math.atan2(A[1]-center[1], A[0]-center[0])
                            ang2 = math.atan2(B[1]-center[1], B[0]-center[0])
                            ang3 = math.atan2(C[1]-center[1], C[0]-center[0])
                            def normalize(a):
                                while a <= -math.pi: a += 2*math.pi
                                while a > math.pi: a -= 2*math.pi
                                return a
                            a2 = normalize(ang2 - ang1)
                            a3 = normalize(ang3 - ang1)
                            angle = abs(a3) if (a2 > 0 and a3 > a2) or (a2 < 0 and a3 < a2) else 2*math.pi - abs(a3)
                            net_lengths[name] += r * angle
                state = None

print("=== Baseband I/Q Pairs (Req: ±20mm matching for +/-) ===")
qsd_names = sorted(list(set(n.split("_")[0] for n in qsd_bb_nets.keys())))
for qsd in qsd_names:
    for pair in ["I", "Q"]:
        lp = net_lengths[f"{qsd}_{pair}+"]
        lm = net_lengths[f"{qsd}_{pair}-"]
        diff = abs(lp - lm)
        print(f"{qsd} {pair} pair: {lp:6.2f} mm vs {lm:6.2f} mm | Diff: {diff:5.2f} mm | {'OK' if diff <= 20 else 'FAIL'}")
    iavg = (net_lengths[f"{qsd}_I+"] + net_lengths[f"{qsd}_I-"]) / 2
    qavg = (net_lengths[f"{qsd}_Q+"] + net_lengths[f"{qsd}_Q-"]) / 2
    idiff = abs(iavg - qavg)
    print(f"{qsd} I vs Q avg: {iavg:6.2f} mm vs {qavg:6.2f} mm | Diff: {idiff:5.2f} mm | (Informational)")
    print("-" * 60)

print("
=== FPGA Clock Phases (Req: ±5mm matching across all phases) ===")
for qsd in qsd_names:
    clks = [net_lengths[f"{qsd}_Ph{i}"] for i in range(4)]
    cmin, cmax = min(clks), max(clks)
    cdiff = cmax - cmin
    print(f"{qsd} Clocks: {' / '.join(f'{c:6.2f}' for c in clks)} | Span: {cdiff:5.2f} mm | {'OK' if cdiff <= 5 else 'FAIL'}")
