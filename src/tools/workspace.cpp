// src/tools/workspace.cpp — YOU IMPLEMENT (spec §4.4).
//
// Workspace::resolve: a purely lexical check, steps 1–6 of the spec.
// Do not touch the filesystem here (no exists(), canonical(), ...).
// The constructor, root() and check_symlinks() are provided in
// workspace_provided.cpp.

#include "tools/workspace.h"

#include <string>
#include <vector>

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    auto reject = [reason](const std::string& why) {
        *reason = why;
        return false;
    };

    if (user_path.empty()) return reject("empty path");
    if (user_path.find('\0') != std::string_view::npos) {
        return reject("path contains a NUL byte");
    }

    try {
        const fs::path given{std::string(user_path)};
        if (given.is_absolute() || given.has_root_path()) {
            return reject("absolute paths are not allowed: " + std::string(user_path));
        }

        std::vector<std::string> kept;
        for (const fs::path& piece : given) {
            const std::string part = piece.string();
            if (part.empty() || part == ".") continue;
            if (part == "..") {
                if (kept.empty()) {
                    return reject("path climbs out of the workspace: " + std::string(user_path));
                }
                kept.pop_back();
                continue;
            }
            kept.push_back(part);
        }

        fs::path result = root();
        for (const std::string& part : kept) result /= part;
        *out = result;
        return true;
    } catch (...) {
        return reject("invalid path: " + std::string(user_path));
    }
}
