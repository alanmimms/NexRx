import re
def get_sexpr_block(content, start_idx):
    stack = 0
    for i in range(start_idx, len(content)):
        if content[i] == '(':
            stack += 1
        elif content[i] == ')':
            stack -= 1
            if stack == 0:
                return content[start_idx:i+1], i+1
    return None, None

with open("hw/PGA.kicad_sch", 'r') as f:
    content = f.read()

match = re.search(r'\(symbol\s+\(lib_id\s+', content)
if match:
    start_pos = match.start()
    print(f"Found at byte {start_pos}: {content[start_pos:start_pos+50]!r}")
    block, _ = get_sexpr_block(content, start_pos)
    print(f"Block starts: {block[:50]!r}")
    m = re.search(r'^\(symbol\s+\(lib_id\s+', block)
    print(f"Regex match: {m}")
else:
    print("Not found!")
