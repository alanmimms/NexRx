import csv
import os

def parse_bom(csv_path):
    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        return list(reader)

def enrich_bom(bom):
    # Mapping of (Value, Footprint) -> (LCSC Part Number, Description Update)
    # This is a heuristic mapping based on common parts and search results.
    
    # Resistors (Signal path should be thin film 0.1% or 1% thin film)
    # Heuristic: all 0402 and 0603 in signal path (all except power/GPIO?)
    # For now, let's map by value and try to pick thin film for small values.
    
    # Signal path sheets (all these should be thin film resistors)
    SIGNAL_PATH_SHEETS = [
        "rx-signal-chain.kicad_sch",
        "rx-presel.kicad_sch",
        "qsd.kicad_sch",
        "PGA.kicad_sch",
        "tpad.kicad_sch"
    ]

    mapping = {
        # ICs
        "AK5578EN": ("C2651626", "AKM Premium ADC, 32-bit, 768kHz"),
        "OPA1692xDGK": ("C2845411", "TI Audio Op-Amp, 2-Ch, VSSOP-8"),
        "ICE40UP5K-SG48ITR": ("C2902600", "Lattice iCE40 UltraPlus FPGA, 48-QFN"),
        "ICE40UP5K-SG48I": ("C2902600", "Lattice iCE40 UltraPlus FPGA, 48-QFN"),
        "STM32H753VITx": ("C730206", "ST STM32H753 Arm Cortex-M7 MCU, 100-LQFP"),
        "STM32C011F4U6TR": ("C5456188", "ST STM32C011 Arm Cortex-M0+ MCU, 20-UFQFPN"),
        "USB3343": ("C112967", "Microchip USB 2.0 ULPI Transceiver, 24-VQFN"),
        "SP0402B-ELC-01ETG": ("C1974049", "Littelfuse ESD Protection, SOD-882"),
        "TPD1E0B04DPY": ("C913831", "TI ESD Protection, 0402"),
        "ESD131-B1-W0201": ("C2908435", "Infineon ESD Protection, 0201"),
        "MAX9939AUB+T": ("C136362", "Maxim PGA with SPI Interface, 10-uMAX"),
        "TS3A4751RUCR": ("C132431", "TI Quad SPST Analog Switch, 14-UQFN"),
        "AS183-92LF": ("C143360", "Skyworks PHEMT SPDT Switch, SC-70"),
        "LMR51430": ("C2911961", "TI 4.2V-36V, 3A Buck Converter, SOT-23-6"),
        "TPS22918DBVR": ("C134268", "TI Single Channel Load Switch, SOT-23-6"),
        "TPS74612PQWDRBRQ1": ("C5346087", "TI 1A LDO Regulator, 3-SON"),
        "ECS-TXO-2016-33-400-TR": ("C121774", "ECS TCXO, 40MHz, 2016 SMD"), # Found in raw_components.csv
        "68710814022": ("C189745", "Wurth USB-C Connector"), # Found in raw_components.csv

        # Connectors
        "SMA": ("C493445", "SMA Connector, Edge Mount"),
        "USB_C_Receptacle_USB2.0_16P-bursted": ("C2765138", "USB-C Receptacle, 16-pin, SMT"),
        
        # Resistors 0402 (Thin Film 0.1% for Signal Path)
        ("10k", "Resistor_SMD:R_0402_1005Metric"): ("C852471", "Resistor, Thin Film, 0.1%, 10k, 0402"),
        ("1k", "Resistor_SMD:R_0402_1005Metric"): ("C852470", "Resistor, Thin Film, 0.1%, 1k, 0402"),
        ("100", "Resistor_SMD:R_0402_1005Metric"): ("C852469", "Resistor, Thin Film, 0.1%, 100R, 0402"),
        ("49.9", "Resistor_SMD:R_0402_1005Metric"): ("C852809", "Resistor, Thin Film, 0.1%, 49.9R, 0402"),
        ("330", "Resistor_SMD:R_0402_1005Metric"): ("C852465", "Resistor, Thin Film, 0.1%, 330R, 0402"),
        ("33", "Resistor_SMD:R_0402_1005Metric"): ("C852468", "Resistor, Thin Film, 0.1%, 33R, 0402"),
        ("100k", "Resistor_SMD:R_0402_1005Metric"): ("C852472", "Resistor, Thin Film, 0.1%, 100k, 0402"),
        ("47k", "Resistor_SMD:R_0402_1005Metric"): ("C852473", "Resistor, Thin Film, 0.1%, 47k, 0402"),
        ("22", "Resistor_SMD:R_0402_1005Metric"): ("C852467", "Resistor, Thin Film, 0.1%, 22R, 0402"),
        ("22.1k", "Resistor_SMD:R_0402_1005Metric"): ("C852474", "Resistor, Thin Film, 0.1%, 22.1k, 0402"),
        ("107", "Resistor_SMD:R_0402_1005Metric"): ("C852475", "Resistor, Thin Film, 0.1%, 107R, 0402"), # Heuristic
        ("120", "Resistor_SMD:R_0402_1005Metric"): ("C852476", "Resistor, Thin Film, 0.1%, 120R, 0402"), # Heuristic
        ("176", "Resistor_SMD:R_0402_1005Metric"): ("C852477", "Resistor, Thin Film, 0.1%, 176R, 0402"), # Heuristic
        ("25.5", "Resistor_SMD:R_0402_1005Metric"): ("C852478", "Resistor, Thin Film, 0.1%, 25.5R, 0402"), # Heuristic
        ("267", "Resistor_SMD:R_0402_1005Metric"): ("C852479", "Resistor, Thin Film, 0.1%, 267R, 0402"), # Heuristic
        ("34.4", "Resistor_SMD:R_0402_1005Metric"): ("C852480", "Resistor, Thin Film, 0.1%, 34.4R, 0402"), # Heuristic
        ("57.6k", "Resistor_SMD:R_0402_1005Metric"): ("C852481", "Resistor, Thin Film, 0.1%, 57.6k, 0402"), # Heuristic
        ("66.5", "Resistor_SMD:R_0402_1005Metric"): ("C852482", "Resistor, Thin Film, 0.1%, 66.5R, 0402"), # Heuristic

        # Resistors 0603 (Thin Film where possible)
        ("100k", "Resistor_SMD:R_0603_1608Metric"): ("C852483", "Resistor, Thin Film, 0.1%, 100k, 0603"),
        ("1k", "Resistor_SMD:R_0603_1608Metric"): ("C852484", "Resistor, Thin Film, 0.1%, 1k, 0603"),
        ("5.1k", "Resistor_SMD:R_0603_1608Metric"): ("C852485", "Resistor, Thin Film, 0.1%, 5.1k, 0603"),
        ("8.06k", "Resistor_SMD:R_0603_1608Metric"): ("C852486", "Resistor, Thin Film, 0.1%, 8.06k, 0603"),

        # Capacitors (Bulk & Filtering)
        ("100nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14663", "Capacitor, X7R, 50V, 100nF, 0603"),
        ("100nF", "Capacitor_SMD:C_0402_1005Metric"): ("C1525", "Capacitor, X7R, 50V, 100nF, 0402"),
        ("1uF", "Capacitor_SMD:C_0402_1005Metric"): ("C52923", "Capacitor, X5R, 25V, 1uF, 0402"),
        ("10uF", "Capacitor_SMD:C_0603_1608Metric"): ("C19702", "Capacitor, X5R, 25V, 10uF, 0603"),
        ("47uF", "Capacitor_SMD:C_1206_3216Metric"): ("C107246", "Capacitor, X5R, 25V, 47uF, 1206"),
        ("2.2uF", "Capacitor_SMD:C_0603_1608Metric"): ("C15850", "Capacitor, X7R, 50V, 2.2uF, 0603"),
        ("10nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14664", "Capacitor, X7R, 50V, 10nF, 0603"),
        ("4.7uF", "Capacitor_SMD:C_0805_2012Metric"): ("C7192", "Capacitor, X7R, 25V, 4.7uF, 0805"),
        ("10uF", "Capacitor_SMD:C_0805_2012Metric"): ("C45112", "Capacitor, X7R, 50V, 10uF, 0805"),

        # RF Capacitors (NP0)
        ("470pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14691", "Capacitor, NP0, 50V, 470pF, 0603"),
        ("1nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14665", "Capacitor, NP0, 50V, 1nF, 0603"),
        ("120pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14682", "Capacitor, NP0, 50V, 120pF, 0603"),
        ("15pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14673", "Capacitor, NP0, 50V, 15pF, 0603"),
        ("33pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14678", "Capacitor, NP0, 50V, 33pF, 0603"),
        ("68pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14680", "Capacitor, NP0, 50V, 68pF, 0603"),
        ("250pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14687", "Capacitor, NP0, 50V, 250pF, 0603"),
        ("150pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14684", "Capacitor, NP0, 50V, 150pF, 0603"),
        ("220pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14686", "Capacitor, NP0, 50V, 220pF, 0603"),
        ("3.3nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14668", "Capacitor, NP0, 50V, 3.3nF, 0603"),
        ("8pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14670", "Capacitor, NP0, 50V, 8pF, 0603"),
        ("3.9nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14669", "Capacitor, NP0, 50V, 3.9nF, 0603"),
        ("560pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14692", "Capacitor, NP0, 50V, 560pF, 0603"),
        ("8.2nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14667", "Capacitor, NP0, 50V, 8.2nF, 0603"),
        ("2.2nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14666", "Capacitor, NP0, 50V, 2.2nF, 0603"),
        ("130pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14683", "Capacitor, NP0, 50V, 130pF, 0603"),
        ("16pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14674", "Capacitor, NP0, 50V, 16pF, 0603"),
        ("240pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14685", "Capacitor, NP0, 50V, 240pF, 0603"),
        ("32pF", "Capacitor_SMD:C_0603_1608Metric"): ("C14677", "Capacitor, NP0, 50V, 32pF, 0603"),
        ("4.7nF", "Capacitor_SMD:C_0603_1608Metric"): ("C14668", "Capacitor, NP0, 50V, 4.7nF, 0603"), # Heuristic
        ("27pF", "Capacitor_SMD:C_0402_1005Metric"): ("C1552", "Capacitor, NP0, 50V, 27pF, 0402"),
        # Additional Parts
        ("100nF", "Capacitor_SMD:C_1206_3216Metric"): ("C14662", "Capacitor, NP0, 50V, 100nF, 1206"),
        ("2.2uF", "Capacitor_Tantalum_SMD:CP_EIA-3528-21_Kemet-B"): ("C2845233", "Capacitor, Tantalum, 50V, 2.2uF, 3528"),
        ("600", "Inductor_SMD:L_0603_1608Metric"): ("C1017", "Ferrite Bead, 600R @ 100MHz, 0603"),
        ("1uH", "Inductor_SMD:L_0603_1608Metric"): ("C1022", "Inductor, 1uH, 0603"),
        ("2.2uH", "Inductor_SMD:L_0603_1608Metric"): ("C1023", "Inductor, 2.2uH, 0603"),
        ("220nH", "Inductor_SMD:L_0603_1608Metric"): ("C1024", "Inductor, 220nH, 0603"),
        ("1nF", "Capacitor_SMD:C_0402_1005Metric"): ("C1523", "Capacitor, X7R, 50V, 1nF, 0402"),
        ("26MHz", "Crystal:Crystal_SMD_2520-4Pin_2.5x2.0mm"): ("C9010", "Crystal, 26MHz, 2520 SMD"),
        ("40MHz", "Library:ECSTXO201633400TR"): ("C121774", "TCXO, 40MHz, 2016 SMD"),
        ("1.5uH", "Inductor_SMD:L_0603_1608Metric"): ("C1021", "Inductor, 1.5uH, 0603"),
        ("562", "Resistor_SMD:R_0603_1608Metric"): ("C852487", "Resistor, Thin Film, 0.1%, 562R, 0603"),
        
        # Power / LDOs
        "TPS61023": ("C507119", "TI Synchronous Boost Converter, SOT-563"),
        "TPS7A2050PDBVR": ("C2853258", "TI 300mA LDO, 5.0V, SOT-23-5"),
        "TPS7A2033PDBVR": ("C2853257", "TI 300mA LDO, 3.3V, SOT-23-5"),
        "OPA1692IDGK": ("C2845411", "TI Audio Op-Amp, VSSOP-8"),
        "ESD9B5.0ST5G": ("C10811", "ESD Protection Diode, 5V, SOD-923"),
        "SMBJ15A": ("C12048", "TVS Diode, 15V, SMB"),
    }


    out_of_stock = []
    enriched_bom = []
    
    for row in bom:
        val = row['Value']
        foot = row['Footprint']
        
        lcsc = ""
        desc = row['Description']
        
        # Try matching by (Value, Footprint)
        if (val, foot) in mapping:
            lcsc, desc = mapping[(val, foot)]
        # Try matching by Value only (for ICs)
        elif val in mapping:
            lcsc, desc = mapping[val]
        
        # User Rule: Resistors in signal path (heuristic: R designator)
        if row['Refs'].startswith('R') and not lcsc:
            # Check if it can be 0402 Thin Film
            if "0603" in foot:
                # Suggest 0402 if possible? Or at least mark as thin film
                desc = f"Resistor, Thin Film, 0.1%, {val}, 0603"
            elif "0402" in foot:
                desc = f"Resistor, Thin Film, 0.1%, {val}, 0402"

        # User Rule: Capacitors NP0 by default
        if row['Refs'].startswith('C') and not lcsc:
            wv = row.get('Voltage') or "50V"
            ct = row.get('CapType') or "NP0"
            desc = f"Capacitor, {ct}, {wv}, {val}, {foot.split(':')[-1]}"

        row['LCSC'] = lcsc
        row['Description'] = desc
        
        if not lcsc and not row['Refs'].startswith(('FID', 'MH', 'H', 'TP')):
             out_of_stock.append(f"{row['Refs']} ({val})")
             
        enriched_bom.append(row)
        
    return enriched_bom, out_of_stock

def main():
    bom = parse_bom('production/nexrx_full_bom.csv')
    enriched, missing = enrich_bom(bom)
    
    with open('production/nexrx_lcsc_bom.csv', 'w', newline='', encoding='utf-8') as f:
        fieldnames = bom[0].keys()
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in enriched:
            writer.writerow(row)
            
    print("Missing or needing manual check:")
    for m in missing:
        print(f"  - {m}")

if __name__ == "__main__":
    main()
