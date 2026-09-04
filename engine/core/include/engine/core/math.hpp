#pragma once

// Diligent normalizes NDC depth to its D3D/HLSL-style [0,1] convention across every backend it
// supports, Vulkan included — GLM defaults to OpenGL's [-1,1] range instead. This must be defined
// before any GLM header is included anywhere in the project (PHASE_1_BRIEF.md §2.3), so every
// other module includes GLM through this header rather than reaching for <glm/glm.hpp> directly.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
