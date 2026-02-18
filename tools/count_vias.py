
import re
import os
import glob

def count_vias():
    net_ids = {
        "QSD0_I+": 20, "QSD0_I-": 21, "QSD0_Q+": 27, "QSD0_Q-": 29,
        "QSD1_I+": 31, "QSD1_I-": 32, "QSD1_Q+": 22, "QSD1_Q-": 23,
        "QSD2_I+": 33, "QSD2_I-": 34, "QSD2_Q+": 25, "QSD2_Q-": 26
    }
    id_to_name = {id: name for name, id in net_ids.items()}
    via_counts = {name: 0 for name in net_ids}

    pcb_file = "hw/NexRx.kicad_pcb"
    if not os.path.exists(pcb_file):
        print(f"Error: {pcb_file} not found.")
        return

    with open(pcb_file, "r") as f:
        state = None
        current_net = None
        for line in f:
            line = line.strip()
            if line.startswith("(via"):
                state = "via"
                current_net = None
            elif state == "via":
                if line.startswith("(net "):
                    m = re.search(r"\(net (\d+)\)", line)
                    if m:
                        current_net = int(m.group(1))
                elif line == ")":
                    if current_net in id_to_name:
                        via_counts[id_to_name[current_net]] += 1
                    state = None

    print("Via Counts for QSD Nets:")
    for name in sorted(via_counts.keys()):
        print(f"{name}: {via_counts[name]}")

if __name__ == "__main__":
    count_vias()
