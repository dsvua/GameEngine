#pragma once

#include <format>
#include <string>

struct Version {
    int major{0};
    int minor{0};
    int patch{0};

    std::string toString(bool addV = true) const
    {
        return std::format("{}{}.{}.{}", addV ? "v" : "", major, minor, patch);
    }
};
