#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };
layout (set = 0, binding = 3) buffer readonly VertexBuffer{ Vertex vertex[]; } ;

void main() {
    vec3 inPosition = vertex[gl_VertexIndex].inPosition;
    gl_Position =  uboMatricesAndUserInput.mProjMatrix * uboMatricesAndUserInput.mViewMatrix * vec4(inPosition, 1.0);
}
