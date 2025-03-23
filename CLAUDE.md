# GameEngine Development Guidelines

## Build Commands
- Build project: `./build.sh`
- Run GameEngine: `./run.sh`
- Always change directory into `./bin` if you want to run `./GameEngine` directly
- Compile shaders only: `cmake --build build --target compile_shaders`

## Code Style
- Use `.hpp/.cpp` for C++ files, `.h` for C-compatible headers
- Class names: PascalCase (e.g., `GfxDevice`)
- Functions/variables: camelCase (e.g., `initDevice()`)
- Constants/Macros: UPPER_CASE (e.g., `VK_CHECK`)
- Indentation: 4 spaces (no tabs)
- Braces: Allman style with braces on new lines
- Error handling: Use assertions for invariants; check return values with `VK_CHECK`
- Add descriptive comments for complex logic
- Include guards: Use `#pragma once`
- Keep lines under 120 characters
- Group imports: Standard library first, then external, then project headers
- Use C++17 standard with tabbed indentation
- Structures should use C-style struct initialization with explicit type
- VK_CHECK_FORCE for fatal errors that require program termination
- Every function declaration must have docstring comments
- Error handling via return values (not exceptions)
- Member variables: prefixed with `m_` (e.g., `m_window`)

## Dependencies
This project uses Vulkan, SDL3, TooManyCooks, and various utility libraries for graphics, math, and asset management. See `/external` directory for details.

## Memory Management
- Use RAII for Vulkan resources when appropriate
- Prefer std::vector for dynamic arrays
- Check all Vulkan API return codes with VK_CHECK variants

## Naming Conventions
- Clear, descriptive names for functions/variables
- Prefix Vulkan handles with "vk" in variable names
- Use common Hungarian notation for Vulkan-specific variables

## Architecture
- Use shader compilation through glslangValidator
- Follow the Vulkan object lifecycle patterns
- Follow KISS principle
- Never modify files in `external/` folder
- Limit SDL to window creation and events polling