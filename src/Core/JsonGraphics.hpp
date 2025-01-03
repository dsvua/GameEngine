#pragma once

#include <nlohmann/json_fwd.hpp>

#include "../Renderer/Color.hpp"

void from_json(const nlohmann::json& j, LinearColor& c);
