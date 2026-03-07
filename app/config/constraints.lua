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

-- Left Sidebar
-- =============================================================================
rule { tags = {"widget.Sidebar", "widget.LeftSidebar"}, apply = {
    anchorLeft = 1.0,
    anchorTop = 1.0,
    anchorBottom = 1.0,
    minWidth = "280",
    maxWidth = "350",
    springX = 1.0,  -- Low strength expansion
    marginInner = 0.02,
    marginOuter = 0.0,
}}

-- Right Sidebar
-- =============================================================================
rule { tags = {"widget.Sidebar", "widget.RightSidebar"}, apply = {
    anchorRight = 1.0,
    anchorTop = 1.0,
    anchorBottom = 1.0,
    minWidth = "150",
    maxWidth = "300",
    springX = 1.0,  -- Low strength expansion
    marginInner = 0.02,
    marginOuter = 0.0,
}}

-- Debug Panel (Active Tags Viewer)
-- =============================================================================
rule { tags = {"widget.DebugPanel"}, apply = {
    anchorRight = 1.0,
    anchorTop = 1.0,
    anchorBottom = 1.0,
    minWidth = "180",
    maxWidth = "350",
    springX = 1.0,  -- Low strength expansion
    marginInner = 0.01,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Center Area (contains Spectrum + Waterfall)
-- =============================================================================
rule { tags = {"widget.CenterArea"}, apply = {
    -- Takes majority of space via high spring strength
    minWidth = "400",
    springX = 10.0, 
    springY = 1.0,
    marginInner = 0.01,
    marginOuter = 0.0,
}}

-- =============================================================================
-- Display Area (Vertical group for Spectrum + Waterfall)
-- =============================================================================
rule { tags = {"widget.DisplayArea"}, apply = {
    anchorTop = 1.0,
    anchorLeft = 1.0,
    anchorRight = 1.0,
    anchorBottom = 1.0,
    springX = 1.0,
    springY = 1.0,
    marginInner = 0.0,
    marginOuter = 0.0,
}}

-- Spectrum Display
-- =============================================================================
rule { tags = {"widget.Spectrum"}, apply = {
    minHeight = "150",
    maxHeight = "600",
    springY = 2.0,  -- High priority for vertical space
}}

-- Waterfall Display
-- =============================================================================
rule { tags = {"widget.Waterfall"}, apply = {
    minHeight = "200",
    springY = 1.0,  -- Shared vertical space
}}

-- =============================================================================
-- Generic Panel (default constraints)
-- =============================================================================
rule { tags = {"widget.Panel"}, apply = {
    marginInner = 0.02,
    marginOuter = 0.01,
}}

-- =============================================================================
-- Widget Type Defaults - ALL sizes come from rules, not hardcoded
-- =============================================================================

-- Buttons
rule { tags = {"widget.Button"}, apply = {
    marginInner = 0.0,
    marginOuter = 0.005,
}}

-- Toggle buttons (same as regular buttons)
rule { tags = {"widget.Toggle"}, apply = {
}}

-- Checkboxes
rule { tags = {"widget.Checkbox"}, apply = {
    height = "18",
}}

-- Sliders
rule { tags = {"widget.Slider"}, apply = {
    height = "8",
}}

-- Progress bars
rule { tags = {"widget.ProgressBar"}, apply = {
    height = "8",
}}

-- Labels
rule { tags = {"widget.Label"}, apply = {
    -- Labels size to content by default, but can be overridden
}}

-- Text inputs
rule { tags = {"widget.TextInput"}, apply = {
    height = "28",
}}

-- Meters
rule { tags = {"widget.Meter"}, apply = {
    height = "28",
}}

-- Separators
rule { tags = {"widget.Separator"}, apply = {
    height = "1",
}}

-- =============================================================================
-- Custom Widget Defaults (manually drawn in main.lua)
-- =============================================================================

-- Frequency display box in sidebar (VFO readout)
rule { tags = {"widget.FrequencyDisplay"}, apply = {
    height = "36",
    -- width is contextual (parent.width - padding), handled by fallback
}}

-- S-Meter LED bar display
rule { tags = {"widget.SMeter"}, apply = {
    height = "28",
    -- width is contextual (parent.width - padding), handled by fallback
}}

-- S-Meter text readout
rule { tags = {"widget.SMeterText"}, apply = {
    height = "20",
    -- width matches meter width, handled by fallback
}}

print("[constraints.lua] Layout constraint rules loaded")
