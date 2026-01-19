--[[
    animate.lua - Animation/Easing System

    Provides smooth animations for UI transitions, value changes, and visual effects.
    Supports multiple keyframes with various easing curves.

    Usage:
        local animate = require("animate")

        -- Simple A→B animation
        animate.to(myTable, "x", 0, 100, 0.3, "easeOut")

        -- Multi-keyframe animation
        animate.keyframes(myTable, "scale", {
            {time = 0, value = 1.0, easing = "easeOut"},
            {time = 0.5, value = 1.2, easing = "easeIn"},
            {time = 1.0, value = 1.0},
        }, 0.4)

        -- In update(dt):
        animate.update(dt)
]]

local Animate = {}

-- ============================================================================
-- Easing Functions
-- Input: t in [0,1], Output: eased t in [0,1]
-- ============================================================================

Animate.Easing = {
    -- Linear (no easing)
    linear = function(t)
        return t
    end,

    -- Quadratic ease-in (accelerating)
    easeIn = function(t)
        return t * t
    end,

    -- Quadratic ease-out (decelerating)
    easeOut = function(t)
        return 1 - (1 - t) * (1 - t)
    end,

    -- Quadratic ease-in-out (S-curve)
    easeInOut = function(t)
        if t < 0.5 then
            return 2 * t * t
        else
            return 1 - 2 * (1 - t) * (1 - t)
        end
    end,

    -- Cubic ease-in
    cubicIn = function(t)
        return t * t * t
    end,

    -- Cubic ease-out
    cubicOut = function(t)
        local u = 1 - t
        return 1 - u * u * u
    end,

    -- Cubic ease-in-out
    cubicInOut = function(t)
        if t < 0.5 then
            return 4 * t * t * t
        else
            local u = 2 * t - 2
            return 1 + 0.5 * u * u * u
        end
    end,

    -- Exponential ease-in
    exponentialIn = function(t)
        if t == 0 then return 0 end
        return math.pow(2, 10 * (t - 1))
    end,

    -- Exponential ease-out
    exponentialOut = function(t)
        if t == 1 then return 1 end
        return 1 - math.pow(2, -10 * t)
    end,

    -- Exponential ease-in-out
    exponentialInOut = function(t)
        if t == 0 then return 0 end
        if t == 1 then return 1 end
        if t < 0.5 then
            return 0.5 * math.pow(2, 20 * t - 10)
        else
            return 1 - 0.5 * math.pow(2, -20 * t + 10)
        end
    end,

    -- Sine ease-in
    sineIn = function(t)
        return 1 - math.cos(t * math.pi / 2)
    end,

    -- Sine ease-out
    sineOut = function(t)
        return math.sin(t * math.pi / 2)
    end,

    -- Sine ease-in-out
    sineInOut = function(t)
        return 0.5 * (1 - math.cos(math.pi * t))
    end,

    -- Elastic ease-out (bouncy overshoot)
    elasticOut = function(t)
        if t == 0 then return 0 end
        if t == 1 then return 1 end
        local p = 0.3
        local s = p / 4
        return math.pow(2, -10 * t) * math.sin((t - s) * (2 * math.pi) / p) + 1
    end,

    -- Back ease-out (slight overshoot)
    backOut = function(t)
        local s = 1.70158
        local u = t - 1
        return u * u * ((s + 1) * u + s) + 1
    end,
}

-- Alias for common names
Animate.Easing.exponential = Animate.Easing.exponentialOut

-- ============================================================================
-- Animation State
-- ============================================================================

-- Active animations: {id -> animation}
Animate.active = {}

-- Counter for generating unique IDs
local nextId = 1

-- ============================================================================
-- Public API
-- ============================================================================

--- Create a simple A→B animation
-- @param target table containing the property to animate
-- @param property property name (string)
-- @param from start value
-- @param to end value
-- @param duration animation duration in seconds
-- @param easing easing function name (default: "linear")
-- @param onComplete optional callback when animation finishes
-- @return animation handle (id string)
function Animate.to(target, property, from, to, duration, easing, onComplete)
    return Animate.keyframes(target, property, {
        {time = 0, value = from, easing = easing or "linear"},
        {time = 1, value = to},
    }, duration, onComplete)
end

--- Create a multi-keyframe animation
-- @param target table containing the property to animate
-- @param property property name (string)
-- @param keyframes array of {time=0-1, value=number, easing=string}
-- @param duration total animation duration in seconds
-- @param onComplete optional callback when animation finishes
-- @return animation handle (id string)
function Animate.keyframes(target, property, keyframes, duration, onComplete)
    -- Validate
    if not target or not property then
        print("[Animate] Error: target and property required")
        return nil
    end
    if not keyframes or #keyframes < 2 then
        print("[Animate] Error: need at least 2 keyframes")
        return nil
    end
    if duration <= 0 then
        -- Instant: just set final value
        target[property] = keyframes[#keyframes].value
        if onComplete then onComplete() end
        return nil
    end

    -- Ensure keyframes are sorted by time
    table.sort(keyframes, function(a, b) return a.time < b.time end)

    -- Generate unique ID
    local id = string.format("anim_%d_%s", nextId, property)
    nextId = nextId + 1

    -- Create animation
    Animate.active[id] = {
        id = id,
        target = target,
        property = property,
        keyframes = keyframes,
        duration = duration,
        elapsed = 0,
        playing = true,
        onComplete = onComplete,
    }

    -- Set initial value
    target[property] = keyframes[1].value

    return id
end

--- Update all active animations
-- Call this from update(dt)
-- @param dt delta time in seconds
function Animate.update(dt)
    local toRemove = {}

    for id, anim in pairs(Animate.active) do
        if anim.playing then
            anim.elapsed = anim.elapsed + dt
            local t = math.min(anim.elapsed / anim.duration, 1)

            -- Interpolate and set value
            anim.target[anim.property] = Animate._interpolate(anim.keyframes, t)

            -- Check for completion
            if t >= 1 then
                table.insert(toRemove, id)
                if anim.onComplete then
                    anim.onComplete()
                end
            end
        end
    end

    -- Remove completed animations
    for _, id in ipairs(toRemove) do
        Animate.active[id] = nil
    end
end

--- Stop an animation
-- @param id animation handle
-- @param skipToEnd if true, set final value immediately
function Animate.stop(id, skipToEnd)
    local anim = Animate.active[id]
    if anim then
        if skipToEnd then
            anim.target[anim.property] = anim.keyframes[#anim.keyframes].value
        end
        Animate.active[id] = nil
    end
end

--- Stop all animations on a specific property
-- @param target the target table
-- @param property the property name
-- @param skipToEnd if true, set final values
function Animate.stopProperty(target, property, skipToEnd)
    local toRemove = {}
    for id, anim in pairs(Animate.active) do
        if anim.target == target and anim.property == property then
            if skipToEnd then
                anim.target[anim.property] = anim.keyframes[#anim.keyframes].value
            end
            table.insert(toRemove, id)
        end
    end
    for _, id in ipairs(toRemove) do
        Animate.active[id] = nil
    end
end

--- Stop all animations on a target
-- @param target the target table
-- @param skipToEnd if true, set final values
function Animate.stopAll(target, skipToEnd)
    local toRemove = {}
    for id, anim in pairs(Animate.active) do
        if anim.target == target then
            if skipToEnd then
                anim.target[anim.property] = anim.keyframes[#anim.keyframes].value
            end
            table.insert(toRemove, id)
        end
    end
    for _, id in ipairs(toRemove) do
        Animate.active[id] = nil
    end
end

--- Pause an animation
-- @param id animation handle
function Animate.pause(id)
    local anim = Animate.active[id]
    if anim then
        anim.playing = false
    end
end

--- Resume a paused animation
-- @param id animation handle
function Animate.resume(id)
    local anim = Animate.active[id]
    if anim then
        anim.playing = true
    end
end

--- Check if a property is currently animating
-- @param target the target table
-- @param property the property name
-- @return boolean
function Animate.isAnimating(target, property)
    for _, anim in pairs(Animate.active) do
        if anim.target == target and anim.property == property and anim.playing then
            return true
        end
    end
    return false
end

--- Get count of active animations
-- @return number
function Animate.getActiveCount()
    local count = 0
    for _ in pairs(Animate.active) do
        count = count + 1
    end
    return count
end

--- Register a custom easing function
-- @param name easing name
-- @param fn function(t) -> eased_t
function Animate.registerEasing(name, fn)
    Animate.Easing[name] = fn
end

-- ============================================================================
-- Internal Functions
-- ============================================================================

--- Interpolate between keyframes at normalized time t
-- @param keyframes sorted array of keyframes
-- @param t normalized time (0-1)
-- @return interpolated value
function Animate._interpolate(keyframes, t)
    -- Find the two keyframes we're between
    local k1 = keyframes[1]
    local k2 = keyframes[#keyframes]

    for i = 1, #keyframes - 1 do
        if t >= keyframes[i].time and t <= keyframes[i + 1].time then
            k1 = keyframes[i]
            k2 = keyframes[i + 1]
            break
        end
    end

    -- Handle edge cases
    if t <= k1.time then return k1.value end
    if t >= k2.time then return k2.value end

    -- Calculate local t within this segment
    local segmentDuration = k2.time - k1.time
    if segmentDuration <= 0 then
        return k2.value
    end
    local localT = (t - k1.time) / segmentDuration

    -- Apply easing
    local easingName = k1.easing or "linear"
    local easingFn = Animate.Easing[easingName] or Animate.Easing.linear
    local easedT = easingFn(localT)

    -- Linear interpolation between values
    return k1.value + (k2.value - k1.value) * easedT
end

return Animate
