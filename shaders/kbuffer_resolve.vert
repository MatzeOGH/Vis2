/**
 * A simple fullscreen-quad vertex shader for the k-buffer resolve
 * @author Gerald Kimmersdorfer, Mathias Hürbe
 * @date 2022
 * @namespace GLSL
 * @class kbuffer_resolve
 */
#version 460
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

vec2 positions[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0),
    vec2(0.0, 0.0)
);

layout (location = 0) out VertexData {
	vec2 texCoords;
} v_out;

void main()
{

	v_out.texCoords = positions[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex] * 2.0 - 1.0, 0.0, 1.0);
}