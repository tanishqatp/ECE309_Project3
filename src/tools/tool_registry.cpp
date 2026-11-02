// src/tools/tool_registry.cpp — YOU IMPLEMENT (spec §4.3).
//
// A hand-written hash table. No std::unordered_map / std::map / std::set.
// Every function below is a stub so the starter compiles. Replace each body.

#include "tools/tool_registry.h"

#include <cstdint>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(8) {}
ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : key) { h ^= c; h *= 1099511628211ULL; }
    return static_cast<std::size_t>(h);
}

std::size_t ToolRegistry::index_for(std::string_view key, std::size_t nbuckets) const noexcept {
    return hash(key) & (nbuckets - 1);
}

void ToolRegistry::rehash(std::size_t new_count) {
    std::vector<std::vector<std::unique_ptr<Tool>>> fresh(new_count);
    for (auto& chain : buckets_)
        for (auto& t : chain) {
            std::size_t i = index_for(t->name(), new_count);
            fresh[i].push_back(std::move(t));
        }
    buckets_ = std::move(fresh);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (tool == nullptr) return false;
    std::string name = tool->name();
    if (find(name) != nullptr) return false;
    if (static_cast<double>(count_ + 1) / static_cast<double>(buckets_.size()) > 1.0)
        rehash(buckets_.size() * 2);
    buckets_[index_for(name, buckets_.size())].push_back(std::move(tool));
    ++count_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const auto& chain = buckets_[index_for(name, buckets_.size())];
    for (const auto& t : chain)
        if (t->name() == name) return t.get();
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept { return count_; }

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& chain : buckets_)
        for (const auto& t : chain) fn(*t);
}

std::size_t ToolRegistry::bucket_count() const noexcept { return buckets_.size(); }

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(count_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& chain : buckets_)
        if (chain.size() > longest) longest = chain.size();
    return longest;
}