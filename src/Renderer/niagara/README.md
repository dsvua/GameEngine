Most files are imported from niagara as is, however, niagara.cpp is split between renderer and gfxDevice. 

Also:
- swapchain has replaced GLFW by SDL.
- scene.h has its types added to GfxTypes and load functions are not used.
- fixed includes of scene.h