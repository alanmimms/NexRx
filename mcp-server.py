import pcbnew
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("NexRx-KiCad-Bridge")

@mcp.tool()
def get_pcb_bounds(board_path: str):
    """Returns the board dimensions in millimeters."""
    board = pcbnew.LoadBoard(board_path)
    rect = board.GetBoardEdgesBoundingBox()
    # KiCad internal units are nanometers (nm), convert to mm
    return {
        "width_mm": rect.GetWidth() / 1e6,
        "height_mm": rect.GetHeight() / 1e6
    }

@mcp.tool()
def check_net_clearance(board_path: str, net_name: str):
    """Checks the current clearance for a specific RF net."""
    board = pcbnew.LoadBoard(board_path)
    net = board.FindNet(net_name)
    # logic to pull DRC constraints from KiCad 9 design rules
    return f"Clearance for {net_name} is currently {board.GetDesignSettings().m_NetClasses.Find(net.GetNetClassName()).GetClearance() / 1e6} mm"

if __name__ == "__main__":
    mcp.run()
