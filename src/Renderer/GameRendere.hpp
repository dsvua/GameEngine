#pragma once

#include "../Math/Math.hpp"

struct SDL_Window;
class Camera;
class GfxDevice;


class GameRenderer {
public:
    void init(GfxDevice gfxDevice, const glm::ivec2& drawImageSize);
private:
}