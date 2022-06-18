#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

layout(location = 0) in vec3 inPosition;

layout (location = 1) out vec4 outColor;

void main() {
    gl_Position =  uboMatricesAndUserInput.mProjMatrix * uboMatricesAndUserInput.mViewMatrix * vec4(inPosition, 1.0);
}
