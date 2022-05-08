#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (lines) in;
layout (triangle_strip, max_vertices = 8) out;

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

layout(location = 1) in vec4 inColor[];
layout(location = 2) in float inRadius[];

layout(location = 0) out vec4 outColor;

void build_house(vec4 position, float radius, vec4 color)
{    
    mat4 pvMatrix = uboMatricesAndUserInput.mProjMatrix * uboMatricesAndUserInput.mViewMatrix;
    //pvMatrix = uboMatricesAndUserInput.mViewMatrix;
    //vec4 positionWithOffest = position + uboMatricesAndUserInput.mCamPos;

    position = vec4(position.xyz, 1.0);

    gl_Position = pvMatrix * (position + vec4(0.0, +radius, 0.0, 0.0));    // 1:center top
    outColor = color;
    EmitVertex();   
    gl_Position = pvMatrix * (position + vec4(+radius, -radius, 0.0, 0.0));    // 2:bottom-right
    outColor = color;
    EmitVertex();
    gl_Position = pvMatrix * (position + vec4(-radius,  -radius, 0.0, 0.0));    // 3:bottom-left
    outColor = color;
    EmitVertex();


    EndPrimitive();
}

void main() {
    //for (int i=0; i<gl_in.length(); i++) {
    //    outColor[i] = fragColor[i];
    //}

    build_house(gl_in[0].gl_Position, inRadius[0], inColor[0]);
    build_house(gl_in[1].gl_Position, inRadius[1], inColor[1]);
}