#define TMC_IMPL

#include "Renderer/Renderer.h"
#include "tmc/ex_cpu.hpp"
#include "Utils/thread_name.hpp"

#ifdef WIN32
/**
 * Windows entry point for the application.
 * Initializes the renderer and runs the main render loop.
 * 
 * @param hInstance Current instance handle
 * @param hPrevInstance Previous instance handle (always NULL)
 * @param lpCmdLine Command line arguments
 * @param nCmdShow Show window command parameter
 * @return Application exit code
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
/**
 * Unix entry point for the application.
 * Initializes the renderer and runs the main render loop.
 * 
 * @param __argc Number of command line arguments
 * @param __argv Array of command line argument strings
 * @return Application exit code
 */
int main(int __argc, const char** __argv)
#endif
{
    tmc::ex_cpu executor;
    hookInitExCpuThreadId(executor);
    executor.init();

    Renderer renderer;

    while (!shouldQuit(renderer.m_gfxDevice))
    {
        renderer.draw();
    }

    renderer.cleanup();
    
    return 0;
}