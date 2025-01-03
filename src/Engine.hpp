#pragma once

#include <string>

#include <glm/vec2.hpp>

// #include "Renderer/Renderer.hpp"
#include "Version.hpp"

struct SDL_Window;

struct Params {
    // windowSize should be set
    // You can also load them in loadAppSettings before they're used
    glm::ivec2 windowSize{800, 600}; // default window size

    std::string appName{"Super Game"};

    Version version{};

    // std::string sourceSubDirName;
    // needed for finding dev files in source directory
    // they should be stored in ${EDBR_SOURCE_ROOT}/games/<sourceSubDirName>/dev/
};

class Engine {
public:

    Engine(const Params& ps);
    void run();
    virtual void onWindowResize(){};

protected:
    // Renderer* renderer;

    SDL_Window* window{nullptr};

    Params params;
    bool vSync{true};

    bool isRunning{false};
    bool gamePaused{false};

};