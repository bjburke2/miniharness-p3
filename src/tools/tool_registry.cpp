// src/tools/tool_registry.cpp — YOU IMPLEMENT (spec §4.3).
//
// A hand-written hash table. No std::unordered_map / std::map / std::set.
// Every function below is a stub so the starter compiles. Replace each body.

#include "tools/tool_registry.h"

#include <cstdint>
#include <string>
#include <utility>

namespace {

constexpr std::size_t kInitialSlots = 16;

}  // namespace

ToolRegistry::ToolRegistry() : slots_(kInitialSlots) {}    // TODO: create your initial buckets
ToolRegistry::~ToolRegistry() = default;   // unique_ptrs in your buckets free the tools

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 0x9E3779B97F4A7C15ULL;
    for (unsigned char c : key) {
        h = (h ^ c) * 0x100000001B3ULL;
        h ^= h >> 29;
    }
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return static_cast<std::size_t>(h);   // TODO: FNV-1a (spec Appendix A) or another real string hash
}

void ToolRegistry::place(std::unique_ptr<Tool> tool, const std::string& name) {
    const std::size_t mask = slots_.size() - 1;
    std::size_t i = hash(name) & mask;
    while (slots_[i]) i = (i + 1) & mask;
    slots_[i] = std::move(tool);
}

void ToolRegistry::grow() {
    std::vector<std::unique_ptr<Tool>> old = std::move(slots_);
    slots_ = std::vector<std::unique_ptr<Tool>>(old.size() * 2);
    for (std::unique_ptr<Tool>& entry : old) {
        if (entry) {
            const std::string name = entry->name();
            place(std::move(entry), name);
        }
    }
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (tool == nullptr) return false;
    const std::string name = tool->name();
    if (find(name) != nullptr) return false;
    if ((used_ + 1) * 2 > slots_.size()) grow();
    place(std::move(tool), name);
    ++used_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const std::size_t mask = slots_.size() - 1;
    std::size_t i = hash(name) & mask;
    while (slots_[i]) {
        if (slots_[i]->name() == name) return slots_[i].get();
        i = (i + 1) & mask;
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return used_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const std::unique_ptr<Tool>& entry : slots_) {
        if (entry) fn(*entry);
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return slots_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(used_) / static_cast<double>(slots_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    const std::size_t mask = slots_.size() - 1;
    std::size_t worst = 0;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i]) continue;
        const std::size_t home = hash(slots_[i]->name()) & mask;
        const std::size_t steps = ((i - home) & mask) + 1;
        if (steps > worst) worst = steps;
    }
    return worst;
}
