-- uitest/config/default.lua

rule {
    -- No id means no specific tags, making it a global fallback
    priority = -100,
    apply = {
        fontSize = 18,
        fontPaths = {
            "fonts/DejaVuSans.ttf"
        },
        springLeft = 1.0,
        springRight = 1.0,
        springTop = 1.0,
        springBottom = 1.0,
        spacing = 0,
        padding = 0,
	label = "?",
	order = -9999,
	parent = "-???undefined-parent???-",
	textColor = {1.0, 1.0, 1.0, 1.0}
    }
}
