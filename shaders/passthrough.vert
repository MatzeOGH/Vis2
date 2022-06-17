#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

struct PushConstants {
	int drawCallIndex;
};

layout(push_constant) uniform PushConstantsBlock { PushConstants pushConstants; };

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inRadius;

layout(location = 1) out vec4 outColor;
layout(location = 2) out float outRadius;



void main() {
    gl_Position =  vec4(inPosition, 1.0);

    outColor.rgb = mix(vec3(1,169,51)/255, vec3(255,0,5)/255, inRadius);
    outColor.a = 0.5; //mix( 0.0,0.3, inRadius);

    outRadius =  0.001; //inRadius / 100; //inRadius/50;

}
