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

void main() {
	int index = gl_InstanceIndex * 3 + gl_VertexIndex;
	vec3 vertPosition = vec3(positions[index], 0.0);

    gl_Position = vec4(vertPosition.xy, 1.0, 1.0);
}