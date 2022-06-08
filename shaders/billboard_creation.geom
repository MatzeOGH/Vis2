#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

// needes to be an adjecency sprite to get access to x0 and x3
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#drawing-line-lists-with-adjacency
layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = 4) out;

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

layout(location = 1) in vec4 inColor[];
layout(location = 2) in float inRadius[];

layout(location = 1) out vec4 outColor;
layout(location = 2) out vec3 outPosA;
layout(location = 3) out vec3 outPosB;
layout(location = 4) out vec2 outRARB;
layout(location = 5) out vec3 outN0;
layout(location = 6) out vec3 outN1;

// Helper function that i used to first test the geometry shader
// May be removed in the future
void construct_simple_billboard(vec4 posA, vec4 posB, float radA, float radB, vec4 eyePos) {
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 va = normalize(posA - posB).xyz;
    vec3 pa = posA.xyz + va * radA;
    vec3 vb = normalize(posB - posA).xyz;
    vec3 pb = posB.xyz + vb * radB;
    
    vec3 c0 = pa + up * radA;
    vec3 c1 = pb + up * radB;
    vec3 c2 = pb - up * radB;
    vec3 c3 = pa - up * radA;

    mat4 pvMatrix = uboMatricesAndUserInput.mProjMatrix * uboMatricesAndUserInput.mViewMatrix;

    vec4 color = inColor[0];
    gl_Position = pvMatrix * vec4(c1, 1.0);
    outColor = color;
    EmitVertex();
    gl_Position = pvMatrix * vec4(c0, 1.0);
    outColor = color;
    EmitVertex();   
    gl_Position = pvMatrix * vec4(c2, 1.0);
    outColor = color;
    EmitVertex();
    gl_Position = pvMatrix * vec4(c3, 1.0);
    outColor = color;
    EmitVertex();
    EndPrimitive();
}

void construct_billboard_for_line(vec4 posA, vec4 posB, float radA, float radB, vec4 eyePos, vec4 camDir) {
    vec3 x0 = posA.xyz;
    vec3 x1 = posB.xyz;
    float r0 = radA;
    float r1 = radB;

    if (r0 > r1) {
        x0 = posB.xyz;
        x1 = posA.xyz;
        r0 = radB;
        r1 = radA;
    }

    vec3 e = eyePos.xyz;
    vec3 d = x1 - x0;
    vec3 d0 = e - x0;
    vec3 d1 = e - x1;


    vec3 u = normalize(cross(d,d0));
    vec3 v0 = normalize(cross(u,d0));
    vec3 v1 = normalize(cross(u,d1));

    float t0 = sqrt(length(d0)*length(d0) - r0*r0);
    float s0 = r0 / t0;
    float r0s = length(d0) * s0;

    float t1 = sqrt(length(d1)*length(d1) - r1*r1);
    float s1 = r1 / t1;
    float r1s = length(d1) * s1;

    vec3 p0 = x0 + r0s*v0;
    vec3 p1 = x0 - r0s*v0;
    vec3 p2 = x1 + r1s*v1;
    vec3 p3 = x1 - r1s*v1;

    float sm = max(s0, s1);
    float r0ss = length(d0) * sm;
    float r1ss = length(d1) * sm;

    vec3 v = camDir.xyz;
    vec3 w = cross(u, v);
    float a0 = dot(normalize(p0-e), normalize(w));
    float a2 = dot(normalize(p2-e), normalize(w));

    vec3 ps = (a0 <= a2) ? p0 : p2;
    float rs = (a0 <= a2) ? r0ss : r1ss;

    float a1 = dot(normalize(p1-e), normalize(w));
    float a3 = dot(normalize(p3-e), normalize(w));

    vec3 pe = (a1 <= a3) ? p3 : p1;
    float re = (a1 <= a3) ? r1ss : r0ss;

    vec3 c0 = ps - rs*u;
    vec3 c1 = ps + rs*u;
    vec3 c2 = pe - re*u;
    vec3 c3 = pe + re*u;
    
    mat4 pvMatrix = uboMatricesAndUserInput.mProjMatrix * uboMatricesAndUserInput.mViewMatrix;
    //pvMatrix = uboMatricesAndUserInput.mProjMatrix;

    gl_Position = pvMatrix * vec4(c0, 1.0);
    d = c0 - e;
    outColor = vec4(d.xyz,0);
    outPosA = x0;
    outPosB = x1;
    outRARB = vec2(r0, r1);
    EmitVertex();   
    gl_Position = pvMatrix * vec4(c1, 1.0);
    d = c1 - e;
    outColor = vec4(d.xyz,0);
    outPosA = x0;
    outPosB = x1;
    outRARB = vec2(r0, r1);
    EmitVertex();
    gl_Position = pvMatrix * vec4(c2, 1.0);
    d = c2 - e;
    outColor = vec4(d.xyz,0);
    outPosA = x0;
    outPosB = x1;
    outRARB = vec2(r0, r1);
    EmitVertex();
    gl_Position = pvMatrix * vec4(c3, 1.0);
    d = c3 - e;
    outColor = vec4(d.xyz,0);
    outPosA = x0;
    outPosB = x1;
    outRARB = vec2(r0, r1);
    EmitVertex();
    EndPrimitive();

}

void main() {
    construct_billboard_for_line(
        gl_in[1].gl_Position,
        gl_in[2].gl_Position,
        inRadius[1],
        inRadius[2],
        uboMatricesAndUserInput.mCamPos,
        uboMatricesAndUserInput.mCamDir
    );
}