/**
 * @file Rule.hpp
 * @brief Rule definition for SetBox configuration system
 */

#pragma once

#include "TagSet.hpp"

#include <sol/sol.hpp>

#include <string>
#include <variant>
#include <unordered_map>
#include <optional>
#include <functional>

namespace NexRx::SetBox {

/**
 * @brief Property value types supported by SetBox
 *
 * Simple types are stored directly. Complex types (tables, functions)
 * are stored as sol::object references.
 */
using PropertyValue = std::variant<
    double,
    std::string,
    bool,
    sol::object  // For tables, functions, or complex Lua values
>;

/**
 * @brief A collection of named property values
 */
using PropertyMap = std::unordered_map<std::string, PropertyValue>;

/**
 * @brief Condition function type
 *
 * Takes a Lua table context and returns true if the rule should apply.
 * Example: function(ctx) return ctx.volume > 0.7 end
 */
using ConditionFunc = std::optional<sol::function>;

/**
 * @brief A SetBox rule that maps tags to properties
 *
 * Rules are matched against the active tag set. When multiple rules match,
 * resolution order is:
 * 1. Number of matching tags (more specific wins)
 * 2. Explicit priority (higher wins)
 * 3. Declaration order (later wins)
 *
 * Example Lua definition:
 *   rule {
 *       tags = {"Button", "Primary", "Sidebar"},
 *       when = function(ctx) return ctx.hovered end,
 *       priority = 10,
 *       apply = {
 *           color = "#3b82f6",
 *           borderRadius = 4,
 *       }
 *   }
 */
struct Rule {
    /// Unique identifier (for debugging/inspection)
    std::string id;

    /// Tags that must be present for this rule to match
    TagSet tags;

    /// Optional condition function for dynamic matching
    ConditionFunc condition;

    /// Explicit priority (default 0, higher wins)
    int priority = 0;

    /// Properties to apply when this rule matches
    PropertyMap properties;

    /// Source file and line (for debugging)
    std::string sourceFile;
    int sourceLine = 0;

    /// Declaration order (set by engine during registration)
    size_t declarationOrder = 0;

    /// Is this rule enabled?
    bool enabled = true;

    /**
     * @brief Check if this rule matches the given active tags
     * @param activeTags Currently active tags
     * @param context Lua table with dynamic state (for condition evaluation)
     * @return Number of matching tags, or 0 if not all required tags present
     */
    size_t matchScore(const TagSet& activeTags, const sol::table& context) const {
        if (!enabled) {
            return 0;
        }

        // All rule tags must be present in active tags
        if (!activeTags.containsAll(tags)) {
            return 0;
        }

        // Evaluate condition if present
        if (condition && condition->valid()) {
            try {
                sol::protected_function_result result = (*condition)(context);
                if (!result.valid() || !result.get<bool>()) {
                    return 0;
                }
            } catch (...) {
                return 0;  // Condition error = no match
            }
        }

        // Return at least 1 for rules with empty tags (global defaults)
        // so they're included in resolve()
        return tags.size() > 0 ? tags.size() : 1;
    }

    /**
     * @brief Comparison for sorting rules by specificity
     *
     * More specific (more tags) > higher priority > later declaration
     */
    struct SpecificityCompare {
        bool operator()(const Rule* a, const Rule* b) const {
            // More tags = more specific = wins
            if (a->tags.size() != b->tags.size()) {
                return a->tags.size() > b->tags.size();
            }
            // Higher priority wins
            if (a->priority != b->priority) {
                return a->priority > b->priority;
            }
            // Later declaration wins
            return a->declarationOrder > b->declarationOrder;
        }
    };
};

} // namespace NexRx::SetBox
