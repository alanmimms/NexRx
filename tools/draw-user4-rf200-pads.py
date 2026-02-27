import pcbnew
import math

def draw_pad_boxes_on_user4():
    board = pcbnew.GetBoard()
    target_class = "rf200"
    
    # Safely get the Layer ID for User.4 dynamically
    layer_id = board.GetLayerID("User.4")
    if layer_id == pcbnew.UNDEFINED_LAYER:
        print("Error: Could not find layer 'User.4'. Please ensure it is enabled in Board Setup.")
        return
        
    # Safely clear any existing selection
    for item in pcbnew.GetCurrentSelection():
        item.ClearSelected()
        
    count = 0
    for footprint in board.GetFootprints():
        for pad in footprint.Pads():
            net = getattr(pad, 'GetNet', lambda: None)()
            if not net: 
                continue
            
            net_name = getattr(net, 'GetNetClassName', getattr(net, 'GetClassName', lambda: ""))()
            if target_class in net_name:
                pos = pad.GetPosition()
                sz = pad.GetSize()
                
                # Safely get rotation angle
                orient = pad.GetOrientation()
                if hasattr(orient, 'AsRadians'):
                    angle_rad = orient.AsRadians()
                else:
                    angle_rad = math.radians(orient / 10.0)
                    
                # Calculate corners of the pad
                dx = sz.x / 2
                dy = sz.y / 2
                corners = [(-dx, -dy), (dx, -dy), (dx, dy), (-dx, dy)]
                pts = []
                
                for cx, cy in corners:
                    rx = cx * math.cos(angle_rad) - cy * math.sin(angle_rad)
                    ry = cx * math.sin(angle_rad) + cy * math.cos(angle_rad)
                    pts.append(pcbnew.VECTOR2I(int(pos.x + rx), int(pos.y + ry)))
                    
                # Draw the 4 lines representing the pad on User.4
                for i in range(4):
                    line = pcbnew.PCB_SHAPE(board)
                    line.SetShape(pcbnew.SHAPE_T_SEGMENT)
                    line.SetStart(pts[i])
                    line.SetEnd(pts[(i+1)%4])
                    line.SetLayer(layer_id)
                    line.SetWidth(pcbnew.FromMM(0.05)) # Very thin reference line
                    
                    board.Add(line)
                    line.SetSelected() # Select it for your UI workflow!
                    
                count += 1
                
    pcbnew.Refresh()
    print(f"NexRx: Drew and selected bounding boxes for {count} pads on User.4.")

if __name__ == "__main__":
    draw_pad_boxes_on_user4()
