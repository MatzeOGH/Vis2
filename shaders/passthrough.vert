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


uint compute_hash(uint a)
{
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return a;
}

void main() {
    gl_Position =  vec4(inPosition, 1.0);

    uint hash = compute_hash(pushConstants.drawCallIndex);
    vec3 color = vec3(float(hash & 255), float((hash >> 8) & 255), float((hash >> 16) & 255)) / 255.0;
    outColor = vec4(color, 1);

    outRadius =  0.005;

}
