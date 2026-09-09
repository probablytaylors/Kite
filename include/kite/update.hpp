#pragma once

#include <string>

namespace kite {

struct UpdateOptions {
    bool check = false;
    bool force = false;
    bool yes = false;
    bool list = false;
    std::string version;
};

int run_update(const UpdateOptions& options);

} // namespace kite
