// src/tools/tool_registry.cpp — YOU IMPLEMENT (spec §4.3).
//
// A hand-written hash table. No std::unordered_map / std::map / std::set.
// Every function below is a stub so the starter compiles. Replace each body.

#include "tools/tool_registry.h"

#include <cstdint>
#include <string>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(8) {}    // TODO: create your initial buckets
ToolRegistry::~ToolRegistry() = default;   // unique_ptrs in your buckets free the tools

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 14695981039346656037ULL;
    for (char c : key) {
        h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        h *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);   // TODO: FNV-1a (spec Appendix A) or another real string hash
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<std::vector<std::unique_ptr<Tool>>> fresh(new_bucket_count);
    for (auto& bucket : buckets_) {
        for (auto& tool : bucket) {
            std::size_t index = hash(tool->name()) % new_bucket_count;
            fresh[index].push_back(std::move(tool));
        }
    }
    buckets_ = std::move(fresh);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false;
    std::string name = tool->name();
    if (find(name) != nullptr) return false;
    if ((count_ + 1) * 4 > buckets_.size() * 3) rehash(buckets_.size() * 2);
    buckets_[hash(name) % buckets_.size()].push_back(std::move(tool));
    ++count_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const auto& bucket = buckets_[hash(name) % buckets_.size()];
    for (const auto& tool : bucket) {
        if (tool->name() == name) return tool.get();
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return count_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& bucket : buckets_) {
        for (const auto& tool : bucket) fn(*tool);
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(count_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& bucket : buckets_) {
        if (bucket.size() > longest) longest = bucket.size();
    }
    return longest;
}
