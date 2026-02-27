import pcbnew

def run():
    board = pcbnew.GetBoard()
    selection = pcbnew.GetCurrentSelection()
    selection.clear()
    
    target_class = "rf200"
    track_count = 0
    pad_count = 0
    
    # 1. Select Tracks
    for track in board.GetTracks():
        if target_class in track.GetNetClassName():
            track.SetSelected()
            track_count += 1
            
    # 2. Select Pads
    # We iterate through footprints, then the pads within them
    for footprint in board.GetFootprints():
        for pad in footprint.Pads():
            # Check the netclass of the pad
            net_info = pad.GetNet()
            if net_info and target_class in net_info.GetNetClassName():
                pad.SetSelected()
                pad_count += 1
            
    pcbnew.Refresh()
    print(f"NexRx Sync: Selected {track_count} tracks and {pad_count} pads in '{target_class}'.")

if __name__ == "__main__":
    run()
