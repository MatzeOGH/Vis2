#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (lines) in;
layout (triangle_strip, max_vertices = 8) out;

layout(location = 1) in vec4 inColor[];
layout(location = 2) in float inRadius[];

layout(location = 0) out vec4 outColor;

void build_house(vec4 position, float radius, vec4 color)
{    
    gl_Position = position + vec4(-radius, -radius, 0.0, 0.0);    // 1:bottom-left
    outColor = color;
    EmitVertex();   
    gl_Position = position + vec4( radius, -radius, 0.0, 0.0);    // 2:bottom-right
    outColor = color;
    EmitVertex();
    gl_Position = position + vec4(-radius,  radius, 0.0, 0.0);    // 3:top-left
    outColor = color;
    EmitVertex();
    gl_Position = position + vec4( radius,  radius, 0.0, 0.0);    // 4:top-right
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