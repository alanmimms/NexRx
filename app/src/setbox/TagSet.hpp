/**
 * @file TagSet.hpp
 * @brief Tag container for SetBox rule matching
 */

#pragma once

#include <string>
#include <unordered_set>
#include <initializer_list>
#include <vector>

namespace NexRx::SetBox {

/**
 * @brief A set of string tags used for rule matching
 *
 * Tags are case-sensitive strings. Order does not matter for matching.
 */
class TagSet {
public:
    TagSet() = default;

    TagSet(std::initializer_list<std::string> tags)
        : tags_(tags) {}

    explicit TagSet(const std::vector<std::string>& tags)
        : tags_(tags.begin(), tags.end()) {}

    void add(const std::string& tag) {
        tags_.insert(tag);
    }

    void remove(const std::string& tag) {
        tags_.erase(tag);
    }

    bool contains(const std::string& tag) const {
        return tags_.contains(tag);
    }

    void clear() {
        tags_.clear();
    }

    size_t size() const {
        return tags_.size();
    }

    bool empty() const {
        return tags_.empty();
    }

    /**
     * @brief Check if all tags in 'required' are present in this set
     * @param required The tags that must be present
     * @return true if all required tags are present
     */
    bool containsAll(const TagSet& required) const {
        for (const auto& tag : required.tags_) {
            if (!tags_.contains(tag)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Count how many tags from 'other' are present in this set
     * @param other Tags to check
     * @return Number of matching tags
     */
    size_t countMatches(const TagSet& other) const {
        size_t count = 0;
        for (const auto& tag : other.tags_) {
            if (tags_.contains(tag)) {
                ++count;
            }
        }
        return count;
    }

    // Iterators for range-based for loops
    auto begin() const { return tags_.begin(); }
    auto end() const { return tags_.end(); }

    // Convert to vector (useful for Lua binding)
    std::vector<std::string> toVector() const {
        return {tags_.begin(), tags_.end()};
    }

private:
    std::unordered_set<std::string> tags_;
};

} // namespace NexRx::SetBox
