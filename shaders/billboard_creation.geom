#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

// needes to be an adjecency sprite to get access to x0 and x3
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#drawing-line-lists-with-adjacency
layout (lines) in;
layout (triangle_strip, max_vertices = 4) out;

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };
layout (set = 0, binding = 3) buffer readonly VertexBuffer{ Vertex vertex[]; } ;

layout(location = 1) in vec4 inColor[];
layout(location = 2) in float inRadius[];
layout(location = 3) in uint inVertexId[];

layout(location = 0) out vec4 outViewRay;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec3 outPosA;
layout(location = 3) out vec3 outPosB;
layout(location = 4) out vec2 outRARB;
layout(location = 5) out vec3 outN0;
layout(location = 6) out vec3 outN1;
layout(location = 7) out vec3 outPosWS;

void construct_billboard_for_line(vec3 posA, vec3 posB, float radA, float radB, vec4 eyePos, vec4 camDir, vec3 posPreA, vec3 posSucB) {

    vec3 x0 = posA;
    vec3 x1 = posB;
    float r0 = radA;
    float r1 = radB;

    if (r0 > r1) {
        x0 = posB;
        x1 = posA;
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

    // clipping
    //vec3 cx0 = x0;
    vec3 cx0 = posPreA;
    vec3 cx1 = x0;
    vec3 cx2 = x1;
    vec3 cx3 = posSucB;
    //vec3 cx3 = x1; //gl_in[3].gl_Position.xyz;

    // find start and end cap flag
    float start = 1.0;
    if (cx0.x < 0.0) {
        start = 0.0;
        cx0 = abs(cx0);
    }
    float end = 1.0;
    if (cx3.x < 0.0) {
        end = 0.0;
        cx3 = abs(cx3);
    }
    
    vec3 n0 = -normalize(cx2 - cx0) * start;
    vec3 n1 = normalize(cx3 - cx1) * end;

    mat4 pvMatrix = uboMatricesAndUserInput.mProjMatrix * uboMatricesAndUserInput.mViewMatrix;
    //pvMatrix = uboMatricesAndUserInput.mProjMatrix;

    vec2 outRadius = vec2(radA, radB);
    gl_Position = pvMatrix * vec4(c0, 1.0);
    outPosWS = c0;
    d = c0 - e;
    outViewRay = vec4(d.xyz,0);
    outColor = inColor[0];
    outPosA = posA.xyz;
    outPosB = posB.xyz;
    outRARB = outRadius;
    outN0 = n0;
    outN1 = n1;
    EmitVertex();

    gl_Position = pvMatrix * vec4(c1, 1.0);
    outPosWS = c1;
    d = c1 - e;
    outViewRay = vec4(d.xyz,0);
    outColor = inColor[0];
    outPosA = posA.xyz;
    outPosB = posB.xyz;
    outRARB = outRadius;
    outN0 = n0;
    outN1 = n1;
    EmitVertex();

    gl_Position = pvMatrix * vec4(c2, 1.0);
    outPosWS = c2;
    d = c2 - e;
    outViewRay = vec4(d.xyz,0);
    outColor = inColor[1];
    outPosA = posA.xyz;
    outPosB = posB.xyz;
    outRARB = outRadius;
    outN0 = n0;
    outN1 = n1;
    EmitVertex();

    gl_Position = pvMatrix * vec4(c3, 1.0);
    outPosWS = c3;
    d = c3 - e;
    outViewRay = vec4(d.xyz,0);
    outColor = inColor[1];
    outPosA = posA.xyz;
    outPosB = posB.xyz;
    outRARB = outRadius;
    outN0 = n0;
    outN1 = n1;
    EmitVertex();
    EndPrimitive();

}

void main() {
    vec3 posA = vertex[inVertexId[0]].inPosition;
    vec3 posB = vertex[inVertexId[1]].inPosition;
    if (posA.x > 0 && posB.x > 0) {
        vec3 posPreA = vertex[inVertexId[0]-2].inPosition;
        vec3 posSucB = vertex[inVertexId[1]+2].inPosition;
        construct_billboard_for_line(
            posA,
            posB,
            inRadius[0],
            inRadius[1],
            uboMatricesAndUserInput.mCamPos,
            uboMatricesAndUserInput.mCamDir,
            posPreA,
            posSucB
        );
    }

}