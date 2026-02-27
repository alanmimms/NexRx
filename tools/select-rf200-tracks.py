import pcbnew

def select_rf200_tracks():
    board = pcbnew.GetBoard()
    target_class = "rf200"
    count = 0
    
    # Safely clear any existing selection using the correct global method
    for item in pcbnew.GetCurrentSelection():
        item.ClearSelected()
        
    # Iterate tracks and vias
    for track in board.GetTracks():
        if hasattr(track, 'GetNetClassName'):
            net_name = track.GetNetClassName()
            if target_class in net_name:
                track.SetSelected()
                count += 1
                
    pcbnew.Refresh()
    print(f"NexRx: Successfully selected {count} tracks in '{target_class}'.")

if __name__ == "__main__":
    select_rf200_tracks()
