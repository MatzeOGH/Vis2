#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main() {
    if (uboMatricesAndUserInput.mUseVertexColorForHelperLines) {
        outColor = inColor;
    } else {
        outColor = uboMatricesAndUserInput.mHelperLineColor;
    }
}