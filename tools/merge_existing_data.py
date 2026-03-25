import csv
import os

def merge_existing_csv():
    candidate_path = "hw/production/candidate_mouser_bom.csv"
    hw_csv_path = "hw/NexRx.csv"
    
    if not os.path.exists(candidate_path) or not os.path.exists(hw_csv_path):
        print("Missing CSV files for merge.")
        return

    # Load hw/NexRx.csv data
    hw_data = {}
    with open(hw_csv_path, 'r', encoding='utf-8') as f:
        # It's tab or comma? The read_file showed tabs or spaces. 
        # Let's try to detect or use DictReader with proper delimiter.
        content = f.read(1024)
        delimiter = '\t' if '\t' in content else ','
        f.seek(0)
        reader = csv.DictReader(f, delimiter=delimiter)
        for row in reader:
            refs = [r.strip() for r in row['Reference'].split(',')]
            for r in refs:
                hw_data[r] = {
                    "Mfr": row.get('Manufacturer_Name', ''),
                    "MPN": row.get('Manufacturer_Part_Number', '') or row.get('MfgPN', ''),
                    "Mouser": row.get('Mouser Part Number', '')
                }

    # Update candidate_mouser_bom.csv
    updated_rows = []
    with open(candidate_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            ref = row['Ref']
            if ref in hw_data:
                d = hw_data[ref]
                if d['MPN'] and not row['Suggested_MPN']:
                    row['Suggested_MPN'] = d['MPN']
                if d['Mouser'] and not row['Suggested_Mouser_PN']:
                    row['Suggested_Mouser_PN'] = d['Mouser']
                if not row['Notes'] and (d['MPN'] or d['Mouser']):
                    row['Notes'] = "Merged from hw/NexRx.csv"
            updated_rows.append(row)

    with open(candidate_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=updated_rows[0].keys())
        writer.writeheader()
        writer.writerows(updated_rows)
    
    print(f"Merged data for {len(hw_data)} references from hw/NexRx.csv")

if __name__ == "__main__":
    merge_existing_csv()
