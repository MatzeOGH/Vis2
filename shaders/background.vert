/**
 * A simple vertex shader that when invoked 6 times outputs a full screen quad
 * @author Gerald Kimmersdorfer, Mathias Hürbe
 * @date 2022
 * @namespace GLSL
 * @class background
 */

#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, 1.0)
);


void main()
{
    gl_Position = vec4(positions[gl_VertexIndex] * 2.0 - 1.0, 0.0, 1.0);
}

