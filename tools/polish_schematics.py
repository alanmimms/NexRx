import re
import glob
import os

def clean_kicad_file(file_path):
    with open(file_path, 'r') as f:
        lines = f.readlines()

    new_lines = []
    skip_until_depth = None
    depth = 0
    
    # We want to identify the start of a (property "JCLPCBpn" ... block
    # and skip everything until that specific block's closing parenthesis is found.
    
    for line in lines:
        stripped = line.strip()
        
        # Track depth based on all parentheses in the line
        line_depth_change = line.count('(') - line.count(')')
        
        if skip_until_depth is not None:
            depth += line_depth_change
            if depth <= skip_until_depth:
                skip_until_depth = None
            continue

        if '(property "JCLPCBpn"' in line:
            skip_until_depth = depth
            depth += line_depth_change
            # If the block started and ended on the same line
            if depth <= skip_until_depth:
                skip_until_depth = None
            continue
            
        new_lines.append(line)
        depth += line_depth_change

    if len(new_lines) != len(lines):
        with open(file_path, 'w') as f:
            f.writelines(new_lines)
        print(f"Cleaned {file_path}")

if __name__ == "__main__":
    files = glob.glob("hw/*.kicad_sch") + glob.glob("hw/*.kicad_pcb")
    for f in files:
        clean_kicad_file(f)
