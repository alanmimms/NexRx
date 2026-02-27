import pcbnew

def select_rf200_pads():
    board = pcbnew.GetBoard()
    target_class = "rf200"
    count = 0
    
    # Safely clear any existing selection
    for item in pcbnew.GetCurrentSelection():
        item.ClearSelected()
        
    # Iterate all footprints and their pads
    for footprint in board.GetFootprints():
        for pad in footprint.Pads():
            net = pad.GetNet()
            if net:
                # Handle KiCad 9's hierarchical netclass names
                net_name = getattr(net, 'GetNetClassName', getattr(net, 'GetClassName', lambda: ""))()
                if target_class in net_name:
                    pad.SetSelected()
                    count += 1
                    
    pcbnew.Refresh()
    print(f"NexRx: Successfully selected {count} pads in '{target_class}'.")

if __name__ == "__main__":
    select_rf200_pads()
