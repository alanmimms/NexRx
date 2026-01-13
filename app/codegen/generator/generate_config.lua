#!/usr/bin/env lua
-- NexRx Configuration Code Generator
--
-- Reads a Lua schema file and generates C++ code for configuration.
-- Usage: lua generate_config.lua <schema.lua> <output_dir>
--
-- Copyright 2026 NexRx Project - MIT License

local function log(msg)
    io.stderr:write("[codegen] " .. msg .. "\n")
end

local function fatal(msg)
    io.stderr:write("[codegen] ERROR: " .. msg .. "\n")
    os.exit(1)
end

-- Parse command line arguments
local schema_path = arg[1]
local output_dir = arg[2]

if not schema_path or not output_dir then
    fatal("Usage: generate_config.lua <schema.lua> <output_dir>")
end

-- Load the schema
log("Loading schema from: " .. schema_path)
local schema_chunk, err = loadfile(schema_path)
if not schema_chunk then
    fatal("Failed to load schema: " .. (err or "unknown error"))
end

local schema = schema_chunk()
if not schema then
    fatal("Schema did not return a table")
end

local class_name = schema.name or "Config"
local namespace = schema.namespace or "nexrx"
local properties = schema.properties or {}

local prop_count = 0
for _ in pairs(properties) do prop_count = prop_count + 1 end
log("Generating " .. class_name .. " with " .. prop_count .. " properties")

-- Helper: Convert property name to C++ identifier
local function to_cpp_id(name)
    return name:gsub("^%l", string.upper)
end

-- Helper: Convert enum name to C++ constant
local function to_enum_const(name)
    return name:upper():gsub("[^%w]", "_")
end

-- Helper: Format number as C++ double literal
local function to_double(n)
    local s = tostring(n)
    if not s:find("[%.]") and not s:find("[eE]") then
        s = s .. ".0"
    end
    return s
end

-- Helper: Get C++ type for property
local function cpp_type(prop)
    if prop.type == "number" then
        return "double"
    elseif prop.type == "boolean" then
        return "bool"
    elseif prop.type == "string" then
        return "std::string"
    elseif prop.type == "choice" then
        return to_cpp_id(prop._name) .. "Choice"
    elseif prop.type == "action" then
        return nil  -- Actions don't have a type
    else
        return "double"
    end
end

-- Helper: Get C++ default value
local function cpp_default(prop)
    if prop.type == "number" then
        return tostring(prop.default or 0.0)
    elseif prop.type == "boolean" then
        return prop.default and "true" or "false"
    elseif prop.type == "string" then
        return '"' .. (prop.default or "") .. '"'
    elseif prop.type == "choice" then
        local choice = prop.default or prop.choices[1]
        return to_cpp_id(prop._name) .. "Choice::" .. to_enum_const(choice)
    else
        return "0.0"
    end
end

-- Helper: Get PropertyType enum value
local function property_type_enum(prop)
    if prop.type == "number" then
        return "PropertyType::Number"
    elseif prop.type == "boolean" then
        return "PropertyType::Boolean"
    elseif prop.type == "string" then
        return "PropertyType::String"
    elseif prop.type == "choice" then
        return "PropertyType::Choice"
    elseif prop.type == "action" then
        return "PropertyType::Action"
    else
        return "PropertyType::Number"
    end
end

-- Collect properties into ordered list with names
local prop_list = {}
for name, prop in pairs(properties) do
    prop._name = name
    table.insert(prop_list, prop)
end
table.sort(prop_list, function(a, b) return a._name < b._name end)

-- Find all choice properties (need enums)
local choice_props = {}
for _, prop in ipairs(prop_list) do
    if prop.type == "choice" then
        table.insert(choice_props, prop)
    end
end

------------------------------------------------------------------------------
-- Generate Header File
------------------------------------------------------------------------------
local function generate_header()
    local lines = {}
    local function emit(s)
        table.insert(lines, s)
    end

    emit("// " .. class_name .. ".hpp - Generated Configuration Class")
    emit("// AUTO-GENERATED FILE - DO NOT EDIT")
    emit("//")
    emit("// Generated from: " .. schema_path:match("[^/]+$"))
    emit("// Copyright 2026 NexRx Project - MIT License")
    emit("")
    emit("#pragma once")
    emit("")
    emit('#include "config/ConfigBase.hpp"')
    emit('#include "config/PropertyMetadata.hpp"')
    emit('#include "config/RemotePropertyHandler.hpp"')
    emit("")
    emit("#include <sol/sol.hpp>")
    emit("#include <string>")
    emit("#include <vector>")
    emit("")
    emit("namespace " .. namespace .. " {")
    emit("")

    -- Generate enums for choice properties
    for _, prop in ipairs(choice_props) do
        local enum_name = to_cpp_id(prop._name) .. "Choice"
        emit("enum class " .. enum_name .. " {")
        for i, choice in ipairs(prop.choices) do
            local comma = (i < #prop.choices) and "," or ""
            emit("    " .. to_enum_const(choice) .. comma)
        end
        emit("};")
        emit("")
    end

    emit("class " .. class_name .. " : public ConfigBase {")
    emit("public:")

    -- Constructor
    emit("    explicit " .. class_name .. "(RemotePropertyHandler& remote);")
    emit("")

    -- Generate getters and setters
    for _, prop in ipairs(prop_list) do
        local name = prop._name
        local ctype = cpp_type(prop)
        local cpp_name = to_cpp_id(name)

        if prop.type == "action" then
            -- Action - just a method
            emit("    // Action: " .. (prop.description or name))
            emit("    void " .. name .. "();")
        else
            -- Property - getter and setter
            emit("    // " .. (prop.description or name))
            if ctype == "std::string" then
                emit("    const " .. ctype .. "& " .. name .. "() const;")
            else
                emit("    " .. ctype .. " " .. name .. "() const;")
            end
            emit("    void set" .. cpp_name .. "(" .. (ctype == "std::string" and "const std::string& " or ctype .. " ") .. "value);")

            -- Remote properties get a refresh method
            if prop.remote then
                emit("    void refresh" .. cpp_name .. "();")
            end
        end
        emit("")
    end

    -- Metadata accessor
    emit("    // Property metadata for UI generation")
    emit("    static const std::vector<PropertyMetadata>& allProperties();")
    emit("")

    -- Lua bindings
    emit("    // Register Lua bindings")
    emit("    static void registerLuaBindings(sol::state& lua, " .. class_name .. "& config);")
    emit("")

    emit("private:")

    -- Member variables
    for _, prop in ipairs(prop_list) do
        if prop.type ~= "action" then
            local ctype = cpp_type(prop)
            local default = cpp_default(prop)
            emit("    " .. ctype .. " " .. prop._name .. "_ = " .. default .. ";")
        end
    end
    emit("    RemotePropertyHandler& remote_;")

    emit("};")
    emit("")

    -- Choice enum to/from string helpers
    for _, prop in ipairs(choice_props) do
        local enum_name = to_cpp_id(prop._name) .. "Choice"
        emit("const char* toString(" .. enum_name .. " value);")
        emit(enum_name .. " " .. prop._name .. "FromString(const std::string& s);")
        emit("")
    end

    emit("} // namespace " .. namespace)

    return table.concat(lines, "\n")
end

------------------------------------------------------------------------------
-- Generate Implementation File
------------------------------------------------------------------------------
local function generate_impl()
    local lines = {}
    local function emit(s)
        table.insert(lines, s)
    end

    emit("// " .. class_name .. ".cpp - Generated Configuration Implementation")
    emit("// AUTO-GENERATED FILE - DO NOT EDIT")
    emit("//")
    emit("// Generated from: " .. schema_path:match("[^/]+$"))
    emit("// Copyright 2026 NexRx Project - MIT License")
    emit("")
    emit('#include "' .. class_name .. '.hpp"')
    emit("")
    emit("#include <algorithm>")
    emit("#include <cmath>")
    emit("")
    emit("namespace " .. namespace .. " {")
    emit("")

    -- Constructor
    emit(class_name .. "::" .. class_name .. "(RemotePropertyHandler& remote)")
    emit("    : remote_(remote)")
    emit("{")
    emit("}")
    emit("")

    -- Generate getters, setters, and actions
    for _, prop in ipairs(prop_list) do
        local name = prop._name
        local ctype = cpp_type(prop)
        local cpp_name = to_cpp_id(name)

        if prop.type == "action" then
            -- Action implementation
            emit("void " .. class_name .. "::" .. name .. "() {")
            emit('    invokeAction("' .. name .. '");')
            emit("}")
        else
            -- Getter
            if ctype == "std::string" then
                emit("const " .. ctype .. "& " .. class_name .. "::" .. name .. "() const {")
            else
                emit(ctype .. " " .. class_name .. "::" .. name .. "() const {")
            end
            emit("    return " .. name .. "_;")
            emit("}")
            emit("")

            -- Setter with validation and remote command
            emit("void " .. class_name .. "::set" .. cpp_name .. "(" ..
                 (ctype == "std::string" and "const std::string& " or ctype .. " ") .. "value) {")

            -- Range validation for numbers
            if prop.type == "number" and prop.range then
                emit("    value = std::clamp(value, " ..
                     to_double(prop.range[1]) .. ", " ..
                     to_double(prop.range[2]) .. ");")
            end

            -- Check for change
            if ctype == "std::string" then
                emit("    if (value == " .. name .. "_) return;")
            else
                emit("    if (value == " .. name .. "_) return;")
            end

            emit("    " .. name .. "_ = value;")

            -- Remote command (send to twin when value changes)
            if prop.remote then
                local remote_name = prop.remote.name or name:upper()
                if prop.type == "choice" then
                    emit('    remote_.setRemote("' .. remote_name .. '", toString(value));')
                else
                    emit('    remote_.setRemote("' .. remote_name .. '", value);')
                end
            end

            emit('    notifyChange("' .. name .. '");')
            emit("}")

            -- Refresh for remote properties
            if prop.remote then
                emit("")
                emit("void " .. class_name .. "::refresh" .. cpp_name .. "() {")
                if prop.type == "number" then
                    emit('    auto val = remote_.getRemoteNumber("' .. (prop.remote.name or name:upper()) .. '");')
                    emit("    if (val) {")
                    emit("        " .. name .. "_ = *val;")
                    emit('        notifyChange("' .. name .. '");')
                    emit("    }")
                elseif prop.type == "string" or prop.type == "choice" then
                    emit('    auto val = remote_.getRemoteString("' .. (prop.remote.name or name:upper()) .. '");')
                    emit("    if (val) {")
                    if prop.type == "choice" then
                        emit("        " .. name .. "_ = " .. name .. "FromString(*val);")
                    else
                        emit("        " .. name .. "_ = *val;")
                    end
                    emit('        notifyChange("' .. name .. '");')
                    emit("    }")
                end
                emit("}")
            end
        end
        emit("")
    end

    -- Metadata table
    emit("const std::vector<PropertyMetadata>& " .. class_name .. "::allProperties() {")
    emit("    static const std::vector<PropertyMetadata> metadata = {")
    for i, prop in ipairs(prop_list) do
        local comma = (i < #prop_list) and "," or ""
        local ui = prop.ui or {}
        local range = prop.range or {0, 1}
        emit("        {")
        emit('            "' .. prop._name .. '",')
        emit("            " .. property_type_enum(prop) .. ",")
        emit('            "' .. (prop.description or "") .. '",')
        emit("            {" .. tostring(range[1] or 0) .. ", " ..
             tostring(range[2] or 1) .. ", " ..
             tostring(range[3] or 0) .. "},")
        emit('            {"' .. (ui.widget or "") .. '", "' ..
             (ui.label or prop._name) .. '", "' ..
             (ui.suffix or "") .. '", "' ..
             (ui.group or "") .. '"},')

        -- Choices
        if prop.type == "choice" and prop.choices then
            emit("            {" .. table.concat(
                (function()
                    local quoted = {}
                    for _, c in ipairs(prop.choices) do
                        table.insert(quoted, '"' .. c .. '"')
                    end
                    return quoted
                end)(), ", ") .. "},")
        else
            emit("            {},")
        end

        emit("            " .. (prop.remote and "true" or "false") .. ",")
        emit("            " .. (prop.readOnly and "true" or "false"))
        emit("        }" .. comma)
    end
    emit("    };")
    emit("    return metadata;")
    emit("}")
    emit("")

    -- Lua bindings
    emit("void " .. class_name .. "::registerLuaBindings(sol::state& lua, " .. class_name .. "& config) {")
    emit('    auto cfg = lua.new_usertype<' .. class_name .. '>("' .. class_name .. '",')
    emit("        sol::no_constructor")
    emit("    );")
    emit("")

    for _, prop in ipairs(prop_list) do
        local name = prop._name
        local cpp_name = to_cpp_id(name)

        if prop.type == "action" then
            emit('    cfg["' .. name .. '"] = &' .. class_name .. '::' .. name .. ';')
        else
            -- Property with getter/setter
            emit('    cfg["' .. name .. '"] = sol::property(')
            emit('        &' .. class_name .. '::' .. name .. ',')
            emit('        &' .. class_name .. '::set' .. cpp_name)
            emit("    );")
        end
    end

    emit("")
    emit('    lua["config"] = &config;')
    emit("}")
    emit("")

    -- Choice enum helpers
    for _, prop in ipairs(choice_props) do
        local enum_name = to_cpp_id(prop._name) .. "Choice"

        -- toString
        emit("const char* toString(" .. enum_name .. " value) {")
        emit("    switch (value) {")
        for _, choice in ipairs(prop.choices) do
            emit("        case " .. enum_name .. "::" .. to_enum_const(choice) .. ': return "' .. choice .. '";')
        end
        emit("    }")
        emit('    return "";')
        emit("}")
        emit("")

        -- fromString
        emit(enum_name .. " " .. prop._name .. "FromString(const std::string& s) {")
        for i, choice in ipairs(prop.choices) do
            local cond = (i == 1) and "if" or "else if"
            emit('    ' .. cond .. ' (s == "' .. choice .. '") return ' .. enum_name .. '::' .. to_enum_const(choice) .. ';')
        end
        emit("    return " .. enum_name .. "::" .. to_enum_const(prop.choices[1]) .. ";")
        emit("}")
        emit("")
    end

    emit("} // namespace " .. namespace)

    return table.concat(lines, "\n")
end

------------------------------------------------------------------------------
-- Write output files
------------------------------------------------------------------------------
local function write_file(path, content)
    local f, err = io.open(path, "w")
    if not f then
        fatal("Cannot write to " .. path .. ": " .. (err or "unknown error"))
    end
    f:write(content)
    f:write("\n")
    f:close()
    log("Wrote: " .. path)
end

-- Ensure output directory exists
os.execute("mkdir -p " .. output_dir)

local header_path = output_dir .. "/" .. class_name .. ".hpp"
local impl_path = output_dir .. "/" .. class_name .. ".cpp"

write_file(header_path, generate_header())
write_file(impl_path, generate_impl())

log("Code generation complete.")
