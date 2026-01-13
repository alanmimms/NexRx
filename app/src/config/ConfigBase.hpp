// NexRx Configuration System - Base Class for Action Injection
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexrx {

class ConfigBase {
public:
    using ActionHandler = std::function<void()>;
    using ChangeCallback = std::function<void(const std::string& propertyName)>;

    virtual ~ConfigBase() = default;

    void registerAction(const std::string& name, ActionHandler handler) {
        actionHandlers_[name] = std::move(handler);
    }

    void onPropertyChange(ChangeCallback callback) {
        changeCallbacks_.push_back(std::move(callback));
    }

protected:
    void invokeAction(const std::string& name) {
        auto it = actionHandlers_.find(name);
        if (it != actionHandlers_.end()) {
            it->second();
        }
    }

    void notifyChange(const std::string& propertyName) {
        for (const auto& cb : changeCallbacks_) {
            cb(propertyName);
        }
    }

private:
    std::unordered_map<std::string, ActionHandler> actionHandlers_;
    std::vector<ChangeCallback> changeCallbacks_;
};

} // namespace nexrx
