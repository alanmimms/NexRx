import pcbnew
import math

def inflate_poly(poly, amount):
    """Robust polygon expansion across KiCad API versions."""
    corner_strat = getattr(pcbnew.SHAPE_POLY_SET, 'PM_ROUND', 1)
    
    if hasattr(poly, 'Inflate'):
        try:
            poly.Inflate(amount, corner_strat, 32)
        except TypeError:
            try:
                poly.Inflate(amount, corner_strat, 32, False)
            except TypeError:
                poly.Inflate(amount)
    elif hasattr(poly, 'Buffer'):
        try:
            poly.Buffer(amount, corner_strat)
        except TypeError:
            poly.Buffer(amount)
    return poly

def get_track_pill_poly(track, clearance):
    """Pure-math construction of the track boundary + clearance."""
    start = track.GetStart()
    end = track.GetEnd()
    
    sx = getattr(start, 'x', getattr(start, 'GetX', lambda: 0)())
    sy = getattr(start, 'y', getattr(start, 'GetY', lambda: 0)())
    ex = getattr(end, 'x', getattr(end, 'GetX', lambda: 0)())
    ey = getattr(end, 'y', getattr(end, 'GetY', lambda: 0)())
    
    dx = ex - sx
    dy = ey - sy
    length = math.hypot(dx, dy)
    
    poly = pcbnew.SHAPE_POLY_SET()
    lc = pcbnew.SHAPE_LINE_CHAIN()
    
    if length == 0:
        lc.Append(int(sx - 10), int(sy - 10))
        lc.Append(int(sx + 10), int(sy - 10))
        lc.Append(int(sx + 10), int(sy + 10))
        lc.Append(int(sx - 10), int(sy + 10))
    else:
        nx = -dy / length * 10
        ny = dx / length * 10
        lc.Append(int(sx + nx), int(sy + ny))
        lc.Append(int(sx - nx), int(sy - ny))
        lc.Append(int(ex - nx), int(ey - ny))
        lc.Append(int(ex + nx), int(ey + ny))
        
    lc.SetClosed(True)
    poly.AddOutline(lc)
    
    inflate_amt = int((track.GetWidth() / 2) + clearance)
    inflate_poly(poly, inflate_amt)
    return poly

def get_pad_poly(pad, clearance):
    """Extracts pad polygon, explicitly providing the required Layer argument for KiCad 9."""
    try:
        # KiCad 9 requires the layer because of complex pad stacks
        layer = pad.GetLayer()
        poly = pad.GetEffectivePolygon(layer)
        inflate_poly(poly, clearance)
        return poly
    except Exception:
        # Ultimate Fallback: pure math bounding box
        poly = pcbnew.SHAPE_POLY_SET()
        bbox = pad.GetBoundingBox()
        x = getattr(bbox, 'GetX', lambda: bbox.GetLeft() if hasattr(bbox, 'GetLeft') else 0)()
        y = getattr(bbox, 'GetY', lambda: bbox.GetTop() if hasattr(bbox, 'GetTop') else 0)()
        w = getattr(bbox, 'GetWidth', lambda: 0)()
        h = getattr(bbox, 'GetHeight', lambda: 0)()
        
        lc = pcbnew.SHAPE_LINE_CHAIN()
        lc.Append(x, y)
        lc.Append(x + w, y)
        lc.Append(x + w, y + h)
        lc.Append(x, y + h)
        lc.SetClosed(True)
        poly.AddOutline(lc)
        inflate_poly(poly, clearance)
        return poly

def rebuild_rf200_keepout(clearance_mm=0.75):
    board = pcbnew.GetBoard()
    target_class = "rf200"
    keepout_name = "rf200-keepout"
    clearance = int(pcbnew.FromMM(clearance_mm))
    
    # 1. Surgical removal of the old zone
    to_remove = []
    for zone in board.Zones():
        name = getattr(zone, 'GetZoneName', getattr(zone, 'GetName', lambda: ""))()
        if name == keepout_name:
            to_remove.append(zone)
            
    for zone in to_remove:
        board.Remove(zone)
        
    # 2. Build the master hull
    combined_poly = pcbnew.SHAPE_POLY_SET()
    
    for track in board.GetTracks():
        net_name = ""
        if hasattr(track, 'GetNetClassName'):
            net_name = track.GetNetClassName()
        elif hasattr(track, 'GetNet') and track.GetNet():
            net_name = getattr(track.GetNet(), 'GetClassName', lambda: "")()
            
        if target_class in net_name:
            poly = get_track_pill_poly(track, clearance)
            for i in range(poly.OutlineCount()):
                combined_poly.AddOutline(poly.Outline(i))
                
    for footprint in board.GetFootprints():
        for pad in footprint.Pads():
            net = getattr(pad, 'GetNet', lambda: None)()
            if net:
                net_name = getattr(net, 'GetNetClassName', getattr(net, 'GetClassName', lambda: ""))()
                if target_class in net_name:
                    poly = get_pad_poly(pad, clearance)
                    for i in range(poly.OutlineCount()):
                        combined_poly.AddOutline(poly.Outline(i))

    # 3. Merge all shapes into one smooth moat
    combined_poly.Simplify()

    # 4. Construct the Keepout
    new_zone = pcbnew.ZONE(board)
    if hasattr(new_zone, 'SetZoneName'):
        new_zone.SetZoneName(keepout_name)
    else:
        new_zone.SetName(keepout_name)
        
    new_zone.SetIsRuleArea(True)
    new_zone.SetDoNotAllowCopperPour(True)
    if hasattr(new_zone, 'SetPriority'):
        new_zone.SetPriority(100)
    
    # Restrict Layers 2 (In1), 3 (In2), 4 (In3)
    lset = pcbnew.LSET()
    lset.AddLayer(1) 
    lset.AddLayer(2)
    lset.AddLayer(3)
    new_zone.SetLayerSet(lset)
    
    new_zone.SetOutline(combined_poly)
    board.Add(new_zone)
    
    pcbnew.Refresh()
    print(f"NexRx: '{keepout_name}' successfully built! Moat = {clearance_mm}mm.")

if __name__ == "__main__":
    rebuild_rf200_keepout(0.75)
