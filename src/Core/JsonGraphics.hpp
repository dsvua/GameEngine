#pragma once

#include <nlohmann/json_fwd.hpp>

#include "../Graphics/Color.hpp"

void from_json(const nlohmann::json& j, LinearColor& c);
