--[[
    Constraint Rules for Layout System

    Each widget's layout is determined by constraint properties resolved via SetBox.
    Constraints use Lua expressions evaluated with parent dimensions in scope.

    Available constraint properties:
    - anchorLeft, anchorRight, anchorTop, anchorBottom: spring strength (0-1) to that edge
    - width, height: Lua expression for size (e.g., "parent.width * 0.2", "200")
    - minWidth, maxWidth, minHeight, maxHeight: size bounds
    - marginInner: percentage padding inside widget (0.02 = 2%)
    - marginOuter: percentage gap outside widget (0.01 = 1%)
    - springX, springY: spring strength for flexible space distribution

    Sizing modes:
    - Fixed: width = "260" (pixels, avoid if possible)
    - Percentage: width = "parent.width * 0.2"
    - Content: width = "content.width" (from children)
    - Spring: springX = 1.0 (share available space proportionally)
]]

-- =============================================================================
-- Top Bar
-- =============================================================================
rule { tags = {"widget.TopBar"}, apply = {
    anchorTop = 1.0,
    anchorLeft = 1.0,
    anchorRight = 1.0,
    height = "32",
    marginInner = 0.0,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Bottom Bar
-- =============================================================================
rule { tags = {"widget.BottomBar"}, apply = {
    anchorBottom = 1.0,
    anchorLeft = 1.0,
    anchorRight = 1.0,
    height = "28",
    marginInner = 0.0,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Left Sidebar
-- =============================================================================
rule { tags = {"widget.Sidebar", "widget.LeftSidebar"}, apply = {
    anchorLeft = 1.0,
    anchorTop = 1.0,
    anchorBottom = 1.0,
    width = "parent.width * 0.15",
    minWidth = "200",
    maxWidth = "400",
    marginInner = 0.02,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Right Sidebar (shares right edge with DebugPanel via springs)
-- =============================================================================
rule { tags = {"widget.Sidebar", "widget.RightSidebar"}, apply = {
    anchorRight = 1.0,
    anchorTop = 1.0,
    anchorBottom = 1.0,
    springX = 1.0,  -- Share space proportionally with siblings
    minWidth = "180",
    maxWidth = "400",
    marginInner = 0.02,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Debug Panel (Active Tags Viewer - shares right edge with RightSidebar)
-- =============================================================================
rule { tags = {"widget.DebugPanel"}, apply = {
    anchorRight = 1.0,
    anchorTop = 1.0,
    anchorBottom = 1.0,
    springX = 1.0,  -- Share space proportionally with siblings
    minWidth = "200",
    maxWidth = "450",
    marginInner = 0.01,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Center Area (contains Spectrum + Waterfall)
-- =============================================================================
rule { tags = {"widget.CenterArea"}, apply = {
    -- Fills remaining space after sidebars
    springX = 1.0,
    springY = 1.0,
    marginInner = 0.01,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Spectrum Display
-- =============================================================================
rule { tags = {"widget.Spectrum"}, apply = {
    anchorTop = 1.0,
    anchorLeft = 1.0,
    anchorRight = 1.0,
    height = "parent.height * 0.35",
    minHeight = "100",
    marginInner = 0.0,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Waterfall Display
-- =============================================================================
rule { tags = {"widget.Waterfall"}, apply = {
    anchorBottom = 1.0,
    anchorLeft = 1.0,
    anchorRight = 1.0,
    -- Fills remaining after spectrum
    springY = 1.0,
    minHeight = "100",
    marginInner = 0.0,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Generic Panel (default constraints)
-- =============================================================================
rule { tags = {"widget.Panel"}, apply = {
    marginInner = 0.02,
    marginOuter = 0.01,
}}

-- =============================================================================
-- Buttons (minimal margins)
-- =============================================================================
rule { tags = {"widget.Button"}, apply = {
    marginInner = 0.0,
    marginOuter = 0.005,
}}

print("[constraints.lua] Layout constraint rules loaded")
