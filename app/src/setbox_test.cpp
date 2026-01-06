/**
 * @file setbox_test.cpp
 * @brief Test harness for SetBox configuration engine
 */

#include "setbox/SetBox.hpp"

#include <iostream>
#include <iomanip>

using namespace NexRx::SetBox;

void printSeparator(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
}

void printProperties(SetBoxEngine& engine) {
    auto props = engine.resolve();
    std::cout << "Resolved properties (" << props.size() << "):" << std::endl;

    for (const auto& [name, value] : props) {
        std::cout << "  " << std::setw(20) << std::left << name << " = ";

        std::visit([](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, double>) {
                std::cout << val << " (number)";
            } else if constexpr (std::is_same_v<T, std::string>) {
                std::cout << "\"" << val << "\" (string)";
            } else if constexpr (std::is_same_v<T, bool>) {
                std::cout << (val ? "true" : "false") << " (bool)";
            } else {
                std::cout << "[complex lua object]";
            }
        }, value);

        std::cout << std::endl;
    }
}

void printMatchingRules(SetBoxEngine& engine) {
    auto rules = engine.matchingRules();
    std::cout << "Matching rules (" << rules.size() << "):" << std::endl;

    for (const auto* rule : rules) {
        std::cout << "  [" << rule->id << "] tags={";
        bool first = true;
        for (const auto& tag : rule->tags) {
            if (!first) std::cout << ", ";
            std::cout << tag;
            first = false;
        }
        std::cout << "} priority=" << rule->priority
                  << " props=" << rule->properties.size() << std::endl;
    }
}

int main() {
    std::cout << "SetBox Engine Test\n" << std::endl;

    SetBoxEngine engine;

    // Register change callback
    engine.onPropertyChange([](const std::string& name, const PropertyValue& value) {
        std::cout << "[CHANGE] " << name << " changed" << std::endl;
    });

    // Load configuration
    printSeparator("Loading Configuration");

    std::string testConfig = R"lua(
-- Base defaults (lowest priority)
rule {
    tags = {},
    priority = -100,
    apply = {
        background = "#1a1a2e",
        foreground = "#ffffff",
        fontSize = 14,
        borderRadius = 4,
    }
}

-- Button base style
rule {
    tags = {"Button"},
    apply = {
        background = "#3b82f6",
        foreground = "#ffffff",
        padding = 8,
        borderRadius = 6,
    }
}

-- Primary button variant
rule {
    tags = {"Button", "Primary"},
    apply = {
        background = "#2563eb",
        fontWeight = "bold",
    }
}

-- Secondary button variant
rule {
    tags = {"Button", "Secondary"},
    apply = {
        background = "#64748b",
    }
}

-- Sidebar context
rule {
    tags = {"Sidebar"},
    apply = {
        background = "#0f172a",
        width = 250,
    }
}

-- Button in sidebar gets different style
rule {
    tags = {"Button", "Sidebar"},
    apply = {
        borderRadius = 0,
        width = "100%",
    }
}

-- Primary button in sidebar (most specific)
rule {
    tags = {"Button", "Primary", "Sidebar"},
    apply = {
        background = "#1d4ed8",
    }
}

-- Experimental override (user can toggle)
rule {
    tags = {"Button", "experimental"},
    priority = 100,
    apply = {
        background = "#dc2626",
        borderRadius = 20,
    }
}

-- Conditional rule (based on dynamic context)
rule {
    tags = {"Button"},
    when = function(ctx) return ctx.hovered == true end,
    priority = 50,
    apply = {
        background = "#60a5fa",
        cursor = "pointer",
    }
}

-- Radio configuration (same system!)
rule {
    tags = {"Radio", "20m"},
    apply = {
        frequency = 14.0e6,
        antenna = "beam",
    }
}

rule {
    tags = {"Radio", "20m", "CW"},
    apply = {
        frequency = 14.035e6,
        mode = "CW",
        filterWidth = 500,
    }
}

rule {
    tags = {"Radio", "40m", "SSB"},
    apply = {
        frequency = 7.2e6,
        mode = "LSB",
        filterWidth = 2400,
    }
}

print("Configuration loaded: " .. #rules_ .. " rules")
)lua";

    // Make rules_ available for the print statement
    engine.lua()["rules_"] = engine.rules();

    if (!engine.loadString(testConfig, "test_config")) {
        std::cerr << "Failed to load config: " << engine.lastError() << std::endl;
        return 1;
    }

    std::cout << "Loaded " << engine.rules().size() << " rules" << std::endl;

    // Test 1: No tags (base defaults only)
    printSeparator("Test 1: No Tags (Base Defaults)");
    engine.setActiveTags({});
    printMatchingRules(engine);
    printProperties(engine);

    // Test 2: Button
    printSeparator("Test 2: Button Tag");
    engine.setActiveTags({"Button"});
    printMatchingRules(engine);
    printProperties(engine);

    // Test 3: Primary Button
    printSeparator("Test 3: Primary Button");
    engine.setActiveTags({"Button", "Primary"});
    printMatchingRules(engine);
    printProperties(engine);

    // Test 4: Primary Button in Sidebar
    printSeparator("Test 4: Primary Button in Sidebar");
    engine.setActiveTags({"Button", "Primary", "Sidebar"});
    printMatchingRules(engine);
    printProperties(engine);

    // Test 5: Toggle experimental
    printSeparator("Test 5: Experimental Override");
    engine.addTag("experimental");
    printMatchingRules(engine);
    printProperties(engine);

    // Test 6: Hover state via context
    printSeparator("Test 6: Hover State (Dynamic Context)");
    engine.setContext("hovered", true);
    printMatchingRules(engine);
    printProperties(engine);

    // Test 7: Radio configuration
    printSeparator("Test 7: Radio Config - 20m CW");
    engine.setActiveTags({"Radio", "20m", "CW"});
    engine.clearContext();
    printMatchingRules(engine);
    printProperties(engine);

    // Test 8: Switch to 40m SSB
    printSeparator("Test 8: Radio Config - 40m SSB");
    engine.setActiveTags({"Radio", "40m", "SSB"});
    printMatchingRules(engine);
    printProperties(engine);

    // Test 9: Specific property access
    printSeparator("Test 9: Direct Property Access");
    std::cout << "frequency = " << engine.getNumber("frequency", 0.0) << std::endl;
    std::cout << "mode = " << engine.getString("mode", "unknown") << std::endl;
    std::cout << "filterWidth = " << engine.getNumber("filterWidth", 0.0) << std::endl;

    std::cout << "\n=== All Tests Complete ===" << std::endl;
    return 0;
}
