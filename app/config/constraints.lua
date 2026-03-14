--[[
    Constraint Rules for Layout System (Pure Spring-Constraint)

    Each widget's layout is determined by spring stiffness properties.
    - math.huge: Rigid (stuck)
    - S > 0: Elastic (proportional to 1/S)
    - S < 0: Repulsive (explosive expansion)
    - S = 0: Neutral (passive)
]]

-- =============================================================================
-- Root Window
-- =============================================================================
rule { id = "id-root-window", apply = {
    width = function(ctx) return ctx.window.width end,
    height = function(ctx) return ctx.window.height end,
    direction = "vertical",
    padding = 0,
    spacing = 0,
}}

-- =============================================================================
-- Top Bar
-- =============================================================================
rule { id = "top-bar", tags = {"widget.TopBar"}, apply = {
    parent = "id-root-window",
    order = 1,
    springTop = math.huge,
    springLeft = math.huge,
    springRight = math.huge,
    height = 32,
}}

-- =============================================================================
-- Main Area (Sidebar + Center + Sidebar)
-- =============================================================================
rule { id = "main-area", apply = {
    parent = "id-root-window",
    order = 2,
    direction = "horizontal",
    springTop = math.huge,
    springBottom = math.huge,
    springLeft = math.huge,
    springRight = math.huge,
    springY = 1.0, -- Stretch vertically in parent
}}

-- Left Sidebar
rule { id = "left-sidebar", tags = {"widget.Sidebar", "widget.LeftSidebar"}, apply = {
    parent = "main-area",
    order = 1,
    direction = "vertical",
    springLeft = math.huge,
    springTop = math.huge,
    springBottom = math.huge,
    minWidth = 280,
    padding = 8,
    spacing = 4,
}}

-- Center Area
rule { id = "center-area", tags = {"widget.CenterArea"}, apply = {
    parent = "main-area",
    order = 2,
    direction = "vertical",
    springLeft = 1.0, springRight = 1.0, -- Expand to fill
    springTop = math.huge,
    springBottom = math.huge,
    minWidth = 400,
}}

-- Right Sidebar
rule { id = "right-sidebar", tags = {"widget.Sidebar", "widget.RightSidebar"}, apply = {
    parent = "main-area",
    order = 3,
    direction = "vertical",
    springRight = math.huge,
    springTop = math.huge,
    springBottom = math.huge,
    minWidth = 150,
    padding = 8,
    spacing = 4,
}}

-- Active Tags Sidebar
rule { id = "active-tags", tags = {"widget.Sidebar", "widget.ActiveTagsSidebar"}, apply = {
    parent = "main-area",
    order = 4,
    direction = "vertical",
    springRight = math.huge,
    springTop = math.huge,
    springBottom = math.huge,
    minWidth = 180,
    padding = 8,
    spacing = 4,
}}

-- =============================================================================
-- Display Area (inside Center Area)
-- =============================================================================
rule { tags = {"widget.DisplayArea"}, apply = {
    direction = "vertical",
    springLeft = math.huge, springRight = math.huge,
    springTop = math.huge, springBottom = math.huge,
}}

-- Spectrum
rule { id = "id-spec", tags = {"widget.Spectrum"}, apply = {
    parent = "center-area",
    order = 1,
    springLeft = math.huge, springRight = math.huge,
    springTop = 1.0, springBottom = 1.0, -- Expand vertically
    minHeight = 150,
}}

-- Waterfall
rule { id = "id-wf", tags = {"widget.Waterfall"}, apply = {
    parent = "center-area",
    order = 2,
    springLeft = math.huge, springRight = math.huge,
    springTop = 1.0, springBottom = 1.0,
    minHeight = 200,
}}

-- =============================================================================
-- Sidebar Content Widgets
-- =============================================================================

rule { tags = {"widget.Sidebar", "widget.Label"}, apply = {
    springLeft = math.huge, springRight = math.huge,
    height = 24,
}}

rule { tags = {"widget.Sidebar", "widget.Button"}, apply = {
    springLeft = math.huge, springRight = math.huge,
    height = 28,
}}

rule { tags = {"widget.Sidebar", "widget.Slider"}, apply = {
    springLeft = math.huge, springRight = math.huge,
    height = 12,
}}

-- Special widgets in sidebar
rule { id = "id-smeter", apply = { parent = "left-sidebar", order = 1 }}
rule { id = "id-rx-freq", apply = { parent = "left-sidebar", order = 2 }}
rule { id = "id-rx-slider", apply = { parent = "left-sidebar", order = 3 }}
rule { id = "id-rx-vol", apply = { parent = "left-sidebar", order = 4 }}
rule { id = "id-rx-gain", apply = { parent = "left-sidebar", order = 5 }}

print("[constraints.lua] Pure Spring-Constraint rules loaded")
