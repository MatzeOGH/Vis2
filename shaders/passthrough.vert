/**
 * A simple passthrough vertex shader to forward data to the geometry shader
 * @author Gerald Kimmersdorfer, Mathias Hürbe
 * @date 2022
 * @namespace GLSL
 * @class tubes3D
 */
#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

layout(location = 0) in vec3 inPosition;
layout(location = 1) in float inCurvature;
layout(location = 2) in float inData;

layout(location = 1) out float outCurvature;
layout(location = 2) out float outData;

void main() {
    gl_Position =  vec4(inPosition, 1.0);
    outData =  inData;
    outCurvature = inCurvature;
}
