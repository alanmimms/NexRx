/**
 * @file SetBox.cpp
 * @brief SetBox configuration engine implementation
 */

#include "setbox/SetBox.hpp"

#include <algorithm>
#include <iostream>

namespace NexRx::SetBox {

SetBoxEngine::SetBoxEngine() {
    // Open standard Lua libraries
    lua_.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::string,
        sol::lib::table,
        sol::lib::math,
        sol::lib::io,
        sol::lib::os
    );

    // Create context table
    context_ = lua_.create_table();

    setupLuaEnvironment();
}

SetBoxEngine::~SetBoxEngine() = default;

void SetBoxEngine::setupLuaEnvironment() {
    // Register the global rule() function
    lua_.set_function("rule", [this](sol::table ruleTable) {
        registerRule(ruleTable);
    });

    // Alternative syntax: rule "[tags]" { properties }
    // This creates a function that returns a function for the second call
    lua_.set_function("rule_shorthand", [this](const std::string& tagSpec, sol::table props) {
        // Parse tag spec like "[Button, Primary]" or "Button, Primary"
        sol::table ruleTable = lua_.create_table();

        // Extract tags from spec
        sol::table tagsTable = lua_.create_table();
        std::string spec = tagSpec;

        // Remove brackets if present
        if (!spec.empty() && spec.front() == '[') {
            spec = spec.substr(1);
        }
        if (!spec.empty() && spec.back() == ']') {
            spec = spec.substr(0, spec.size() - 1);
        }

        // Split by comma and trim
        size_t idx = 1;
        size_t start = 0;
        while (start < spec.size()) {
            size_t end = spec.find(',', start);
            if (end == std::string::npos) {
                end = spec.size();
            }

            std::string tag = spec.substr(start, end - start);
            // Trim whitespace
            size_t tagStart = tag.find_first_not_of(" \t");
            size_t tagEnd = tag.find_last_not_of(" \t");
            if (tagStart != std::string::npos && tagEnd != std::string::npos) {
                tag = tag.substr(tagStart, tagEnd - tagStart + 1);
                tagsTable[idx++] = tag;
            }

            start = end + 1;
        }

        ruleTable["tags"] = tagsTable;
        ruleTable["apply"] = props;

        registerRule(ruleTable);
    });

    // Helper for conditional tags: tag ? expr
    // Users can write: { tags = {"Button", pressed and "Pressed"} }
    // Where 'pressed' is a context boolean

    // Provide access to context in Lua
    lua_["ctx"] = context_;
}

void SetBoxEngine::registerRule(const sol::table& ruleTable) {
    Rule rule;
    rule.declarationOrder = nextRuleOrder_++;

    // Get tags
    sol::optional<sol::table> tagsOpt = ruleTable["tags"];
    if (tagsOpt) {
        for (const auto& [key, value] : *tagsOpt) {
            if (value.is<std::string>()) {
                rule.tags.add(value.as<std::string>());
            }
        }
    }

    // Alternative: single tag string
    sol::optional<std::string> tagStringOpt = ruleTable["tag"];
    if (tagStringOpt) {
        rule.tags.add(*tagStringOpt);
    }

    // Get condition function
    sol::optional<sol::function> whenOpt = ruleTable["when"];
    if (whenOpt) {
        rule.condition = *whenOpt;
    }

    // Get priority
    sol::optional<int> priorityOpt = ruleTable["priority"];
    if (priorityOpt) {
        rule.priority = *priorityOpt;
    }

    // Get enabled state
    sol::optional<bool> enabledOpt = ruleTable["enabled"];
    if (enabledOpt) {
        rule.enabled = *enabledOpt;
    }

    // Get ID
    sol::optional<std::string> idOpt = ruleTable["id"];
    if (idOpt) {
        rule.id = *idOpt;
    } else {
        rule.id = "rule_" + std::to_string(rule.declarationOrder);
    }

    // Get properties from 'apply' table
    sol::optional<sol::table> applyOpt = ruleTable["apply"];
    if (applyOpt) {
        for (const auto& [key, value] : *applyOpt) {
            if (!key.is<std::string>()) continue;

            std::string propName = key.as<std::string>();

            if (value.is<double>()) {
                rule.properties[propName] = value.as<double>();
            } else if (value.is<bool>()) {
                rule.properties[propName] = value.as<bool>();
            } else if (value.is<std::string>()) {
                rule.properties[propName] = value.as<std::string>();
            } else {
                // Store complex types as sol::object
                rule.properties[propName] = sol::object(value);
            }
        }
    }

    // Also check for properties directly in rule table (shorthand)
    for (const auto& [key, value] : ruleTable) {
        if (!key.is<std::string>()) continue;

        std::string keyStr = key.as<std::string>();
        // Skip known rule metadata keys
        if (keyStr == "tags" || keyStr == "tag" || keyStr == "when" ||
            keyStr == "priority" || keyStr == "enabled" || keyStr == "id" ||
            keyStr == "apply") {
            continue;
        }

        // This is a property
        if (value.is<double>()) {
            rule.properties[keyStr] = value.as<double>();
        } else if (value.is<bool>()) {
            rule.properties[keyStr] = value.as<bool>();
        } else if (value.is<std::string>()) {
            rule.properties[keyStr] = value.as<std::string>();
        } else {
            rule.properties[keyStr] = sol::object(value);
        }
    }

    rules_.push_back(std::move(rule));
    invalidateCache();
}

bool SetBoxEngine::loadFile(const std::filesystem::path& path) {
    try {
        sol::protected_function_result result = lua_.safe_script_file(
            path.string(),
            sol::script_pass_on_error
        );

        if (!result.valid()) {
            sol::error err = result;
            lastError_ = err.what();
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        lastError_ = e.what();
        return false;
    }
}

bool SetBoxEngine::loadString(const std::string& code, const std::string& chunkName) {
    try {
        sol::protected_function_result result = lua_.safe_script(
            code,
            sol::script_pass_on_error,
            chunkName
        );

        if (!result.valid()) {
            sol::error err = result;
            lastError_ = err.what();
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        lastError_ = e.what();
        return false;
    }
}

void SetBoxEngine::addPackagePath(const std::filesystem::path& path) {
    std::string currentPath = lua_["package"]["path"];
    currentPath += ";" + path.string() + "/?.lua";
    lua_["package"]["path"] = currentPath;
}

void SetBoxEngine::setActiveTags(const TagSet& tags) {
    if (activeTags_.toVector() != tags.toVector()) {
        activeTags_ = tags;
        invalidateCache();
    }
}

void SetBoxEngine::addTag(const std::string& tag) {
    if (!activeTags_.contains(tag)) {
        activeTags_.add(tag);
        invalidateCache();
    }
}

void SetBoxEngine::removeTag(const std::string& tag) {
    if (activeTags_.contains(tag)) {
        activeTags_.remove(tag);
        invalidateCache();
    }
}

void SetBoxEngine::toggleTag(const std::string& tag) {
    if (activeTags_.contains(tag)) {
        activeTags_.remove(tag);
    } else {
        activeTags_.add(tag);
    }
    invalidateCache();
}

bool SetBoxEngine::hasTag(const std::string& tag) const {
    return activeTags_.contains(tag);
}

void SetBoxEngine::setContext(const std::string& name, double value) {
    context_[name] = value;
    invalidateCache();
}

void SetBoxEngine::setContext(const std::string& name, bool value) {
    context_[name] = value;
    invalidateCache();
}

void SetBoxEngine::setContext(const std::string& name, const std::string& value) {
    context_[name] = value;
    invalidateCache();
}

void SetBoxEngine::clearContext() {
    context_ = lua_.create_table();
    lua_["ctx"] = context_;
    invalidateCache();
}

void SetBoxEngine::invalidateCache() {
    PropertyMap oldProps = std::move(cachedProperties_);
    cacheValid_ = false;

    // Re-resolve and notify changes
    PropertyMap newProps = resolve();

    if (!changeCallbacks_.empty()) {
        notifyChanges(oldProps, newProps);
    }
}

void SetBoxEngine::notifyChanges(const PropertyMap& oldProps, const PropertyMap& newProps) {
    // Find changed properties
    for (const auto& [name, value] : newProps) {
        auto it = oldProps.find(name);
        bool changed = (it == oldProps.end()) || (it->second.index() != value.index());

        // Simple comparison for basic types
        if (!changed && it != oldProps.end()) {
            std::visit([&](auto&& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, sol::object>) {
                    changed = true;  // Always notify for complex types
                } else {
                    changed = (std::get<T>(it->second) != val);
                }
            }, value);
        }

        if (changed) {
            for (const auto& callback : changeCallbacks_) {
                callback(name, value);
            }
        }
    }

    // Check for removed properties
    for (const auto& [name, value] : oldProps) {
        if (newProps.find(name) == newProps.end()) {
            // Property was removed - notify with empty value
            // For now, we just don't notify removals
        }
    }
}

PropertyMap SetBoxEngine::resolve() {
    if (cacheValid_) {
        return cachedProperties_;
    }

    // Collect matching rules
    std::vector<const Rule*> matching;
    for (const auto& rule : rules_) {
        if (rule.matchScore(activeTags_, context_) > 0) {
            matching.push_back(&rule);
        }
    }

    // Sort by specificity (most specific first)
    std::sort(matching.begin(), matching.end(), Rule::SpecificityCompare{});

    // Merge properties (later in sorted order = higher priority = wins)
    // So we iterate in reverse to let high-priority rules override
    PropertyMap result;
    for (auto it = matching.rbegin(); it != matching.rend(); ++it) {
        for (const auto& [name, value] : (*it)->properties) {
            result[name] = value;
        }
    }

    cachedProperties_ = result;
    cacheValid_ = true;

    return result;
}

std::optional<PropertyValue> SetBoxEngine::get(const std::string& name) {
    PropertyMap props = resolve();
    auto it = props.find(name);
    if (it != props.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<double> SetBoxEngine::getNumber(const std::string& name) {
    auto val = get(name);
    if (val && std::holds_alternative<double>(*val)) {
        return std::get<double>(*val);
    }
    return std::nullopt;
}

std::optional<std::string> SetBoxEngine::getString(const std::string& name) {
    auto val = get(name);
    if (val && std::holds_alternative<std::string>(*val)) {
        return std::get<std::string>(*val);
    }
    return std::nullopt;
}

std::optional<bool> SetBoxEngine::getBool(const std::string& name) {
    auto val = get(name);
    if (val && std::holds_alternative<bool>(*val)) {
        return std::get<bool>(*val);
    }
    return std::nullopt;
}

double SetBoxEngine::getNumber(const std::string& name, double defaultVal) {
    return getNumber(name).value_or(defaultVal);
}

std::string SetBoxEngine::getString(const std::string& name, const std::string& defaultVal) {
    return getString(name).value_or(defaultVal);
}

bool SetBoxEngine::getBool(const std::string& name, bool defaultVal) {
    return getBool(name).value_or(defaultVal);
}

void SetBoxEngine::onPropertyChange(PropertyChangeCallback callback) {
    changeCallbacks_.push_back(std::move(callback));
}

std::vector<const Rule*> SetBoxEngine::matchingRules() {
    std::vector<const Rule*> matching;
    for (const auto& rule : rules_) {
        if (rule.matchScore(activeTags_, context_) > 0) {
            matching.push_back(&rule);
        }
    }
    std::sort(matching.begin(), matching.end(), Rule::SpecificityCompare{});
    return matching;
}

} // namespace NexRx::SetBox
