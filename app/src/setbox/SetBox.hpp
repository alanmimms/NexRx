/**
 * @file SetBox.hpp
 * @brief SetBox configuration engine - tag-based property resolution
 */

#pragma once

#include "TagSet.hpp"
#include "Rule.hpp"

#include <sol/sol.hpp>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>

namespace NexRx::SetBox {

/**
 * @brief Callback for when properties change
 */
using PropertyChangeCallback = std::function<void(const std::string& name, const PropertyValue& value)>;

/**
 * @brief SetBox configuration engine
 *
 * Manages Lua state, rule registration, and property resolution.
 * The SetBox system provides a unified configuration mechanism where:
 *
 * - Rules map tag combinations to property values
 * - Active tags determine which rules match
 * - Resolution follows: specificity (tag count) > priority > declaration order
 * - Properties can be queried by name, returning the resolved value
 *
 * Usage:
 *   SetBoxEngine engine;
 *   engine.loadFile("config/base/defaults.lua");
 *   engine.loadFile("config/user/overrides.lua");
 *
 *   engine.setActiveTags({"Button", "Primary", "experimental"});
 *
 *   auto color = engine.getString("color");
 *   auto size = engine.getNumber("borderRadius");
 */
class SetBoxEngine {
public:
    SetBoxEngine();
    ~SetBoxEngine();

    // Non-copyable, movable
    SetBoxEngine(const SetBoxEngine&) = delete;
    SetBoxEngine& operator=(const SetBoxEngine&) = delete;
    SetBoxEngine(SetBoxEngine&&) = default;
    SetBoxEngine& operator=(SetBoxEngine&&) = default;

    // -------------------------------------------------------------------------
    // Configuration Loading
    // -------------------------------------------------------------------------

    /**
     * @brief Load and execute a Lua configuration file
     * @param path Path to .lua file
     * @return true on success
     *
     * Files can use dofile() to include other files.
     * Rules are registered via the global rule() function.
     */
    bool loadFile(const std::filesystem::path& path);

    /**
     * @brief Execute Lua code directly
     * @param code Lua source code
     * @param chunkName Name for error messages
     * @return true on success
     */
    bool loadString(const std::string& code, const std::string& chunkName = "string");

    /**
     * @brief Add a path to the Lua package search path
     * @param path Directory to add
     */
    void addPackagePath(const std::filesystem::path& path);

    /**
     * @brief Get the last error message
     */
    const std::string& lastError() const { return lastError_; }

    // -------------------------------------------------------------------------
    // Tag Management
    // -------------------------------------------------------------------------

    /**
     * @brief Set the complete active tag set
     *
     * Triggers property re-resolution and change callbacks.
     */
    void setActiveTags(const TagSet& tags);

    /**
     * @brief Add a tag to the active set
     */
    void addTag(const std::string& tag);

    /**
     * @brief Remove a tag from the active set
     */
    void removeTag(const std::string& tag);

    /**
     * @brief Toggle a tag (add if absent, remove if present)
     */
    void toggleTag(const std::string& tag);

    /**
     * @brief Check if a tag is active
     */
    bool hasTag(const std::string& tag) const;

    /**
     * @brief Get all active tags
     */
    const TagSet& activeTags() const { return activeTags_; }

    // -------------------------------------------------------------------------
    // Dynamic Context
    // -------------------------------------------------------------------------

    /**
     * @brief Set a context value for condition evaluation
     * @param name Context key
     * @param value Context value
     *
     * Context is passed to rule condition functions:
     *   when = function(ctx) return ctx.hovered end
     */
    void setContext(const std::string& name, double value);
    void setContext(const std::string& name, bool value);
    void setContext(const std::string& name, const std::string& value);

    /**
     * @brief Clear all context values
     */
    void clearContext();

    // -------------------------------------------------------------------------
    // Property Resolution
    // -------------------------------------------------------------------------

    /**
     * @brief Resolve and return all properties for current tags/context
     * @return Merged property map from all matching rules
     */
    PropertyMap resolve();

    /**
     * @brief Get a specific property value
     * @param name Property name
     * @return Property value or std::nullopt if not defined
     */
    std::optional<PropertyValue> get(const std::string& name);

    /**
     * @brief Get a property as a specific type
     */
    std::optional<double> getNumber(const std::string& name);
    std::optional<std::string> getString(const std::string& name);
    std::optional<bool> getBool(const std::string& name);

    /**
     * @brief Get a property with a default value
     */
    double getNumber(const std::string& name, double defaultVal);
    std::string getString(const std::string& name, const std::string& defaultVal);
    bool getBool(const std::string& name, bool defaultVal);

    // -------------------------------------------------------------------------
    // Change Notification
    // -------------------------------------------------------------------------

    /**
     * @brief Register a callback for property changes
     * @param callback Function to call when any property changes
     */
    void onPropertyChange(PropertyChangeCallback callback);

    // -------------------------------------------------------------------------
    // Inspection
    // -------------------------------------------------------------------------

    /**
     * @brief Get all registered rules
     */
    const std::vector<Rule>& rules() const { return rules_; }

    /**
     * @brief Get rules that match current tags/context
     */
    std::vector<const Rule*> matchingRules();

    /**
     * @brief Get the Lua state (for advanced use)
     */
    sol::state& lua() { return lua_; }

private:
    void setupLuaEnvironment();
    void registerRule(const sol::table& ruleTable);
    void invalidateCache();
    void notifyChanges(const PropertyMap& oldProps, const PropertyMap& newProps);

    sol::state lua_;
    std::vector<Rule> rules_;
    TagSet activeTags_;
    sol::table context_;

    // Cached resolution
    PropertyMap cachedProperties_;
    bool cacheValid_ = false;

    std::vector<PropertyChangeCallback> changeCallbacks_;
    std::string lastError_;
    size_t nextRuleOrder_ = 0;
};

} // namespace NexRx::SetBox
