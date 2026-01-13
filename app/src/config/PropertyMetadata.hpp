// NexRx Configuration System - Property Metadata Structures
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <limits>
#include <string>
#include <vector>

namespace nexrx {

enum class PropertyType {
    Number,
    Boolean,
    String,
    Choice,
    Action
};

struct PropertyRange {
    double min = 0.0;
    double max = 1.0;
    double step = 0.0;  // 0 = continuous

    static PropertyRange none() {
        return {std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::max(), 0.0};
    }

    static PropertyRange unit() {
        return {0.0, 1.0, 0.0};
    }

    static PropertyRange frequency() {
        return {1e6, 30e6, 1.0};
    }
};

struct PropertyUI {
    std::string widget;   // "slider", "dropdown", "button", "entry", "checkbox"
    std::string label;    // Display label
    std::string suffix;   // Unit suffix (e.g., "Hz", "dB")
    std::string group;    // Grouping for UI organization
};

struct PropertyMetadata {
    std::string name;
    PropertyType type = PropertyType::Number;
    std::string description;
    PropertyRange range;
    PropertyUI ui;
    std::vector<std::string> choices;  // For Choice type
    bool isRemote = false;             // Backed by TCP command
    bool readOnly = false;

    // Helper to check if property has valid range
    bool hasRange() const {
        return range.min < range.max &&
               range.min > std::numeric_limits<double>::lowest() / 2;
    }
};

// Helper to convert PropertyType to string
inline const char* propertyTypeToString(PropertyType type) {
    switch (type) {
        case PropertyType::Number: return "number";
        case PropertyType::Boolean: return "boolean";
        case PropertyType::String: return "string";
        case PropertyType::Choice: return "choice";
        case PropertyType::Action: return "action";
    }
    return "unknown";
}

// Helper to convert string to PropertyType
inline PropertyType stringToPropertyType(const std::string& s) {
    if (s == "number") return PropertyType::Number;
    if (s == "boolean") return PropertyType::Boolean;
    if (s == "string") return PropertyType::String;
    if (s == "choice") return PropertyType::Choice;
    if (s == "action") return PropertyType::Action;
    return PropertyType::String;
}

} // namespace nexrx
