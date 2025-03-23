#define TMC_IMPL

#include "Renderer/Renderer.h"
#include "tmc/ex_cpu.hpp"
#include "Utils/thread_name.hpp"

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main(int __argc, const char** __argv)
#endif
{
    tmc::ex_cpu executor;
    hookInitExCpuThreadId(executor);
    executor.init();

    Renderer renderer;

    while (!shouldQuit())
    {
        printf("Calling renderer.draw() \n");
        renderer.draw();
    }

    renderer.cleanup();
    
    return 0;
}