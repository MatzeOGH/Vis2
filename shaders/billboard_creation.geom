#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

// needes to be an adjecency sprite to get access to x0 and x3
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#drawing-line-lists-with-adjacency
layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = 4) out;

struct PushConstants {
	int drawCallIndex;
};

layout(push_constant) uniform PushConstantsBlock { PushConstants pushConstants; };

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

layout(location = 2) in float inRadius[];

layout(location = 0) out vec4 outViewRay;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec3 outPosA;
layout(location = 3) out vec3 outPosB;
layout(location = 4) out vec2 outRARB;
layout(location = 5) out vec3 outN0;
layout(location = 6) out vec3 outN1;
layout(location = 7) out vec3 outPosWS;

void construct_billboard_for_line(vec4 posA, vec4 posB, float radA, float radB, vec4 eyePos, vec4 camDir, vec4 posAPre, vec4 posBSuc, vec4 colA, vec4 colB) {

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

    // clipping
    vec3 cx0 = posAPre.xyz;
    vec3 cx1 = x0;
    vec3 cx2 = x1;
    vec3 cx3 = posBSuc.xyz;

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
    outColor = colA;
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
    outColor = colA;
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
    outColor = colB;
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
    outColor = colB;
    outPosA = posA.xyz;
    outPosB = posB.xyz;
    outRARB = outRadius;
    outN0 = n0;
    outN1 = n1;
    EmitVertex();
    EndPrimitive();

}

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

vec3 color_from_id_hash(uint a) {
    uint hash = compute_hash(a);
    return vec3(float(hash & 255), float((hash >> 8) & 255), float((hash >> 16) & 255)) / 255.0;
}

void main() {

    vec4 colA = vec4(1.0);
    vec4 colB = vec4(1.0);
    float radA = 0.0;
    float radB = 0.0;

    if (uboMatricesAndUserInput.mVertexColorMode == 0) { // static
        colA.rgb = uboMatricesAndUserInput.mVertexColorMin.rgb;
        colB.rgb = uboMatricesAndUserInput.mVertexColorMin.rgb;
    } else if (uboMatricesAndUserInput.mVertexColorMode == 1) {  // based on velocity
        colA.rgb = mix(uboMatricesAndUserInput.mVertexColorMin.rgb, uboMatricesAndUserInput.mVertexColorMax.rgb, inRadius[1]);
        colB.rgb = mix(uboMatricesAndUserInput.mVertexColorMin.rgb, uboMatricesAndUserInput.mVertexColorMax.rgb, inRadius[2]);
    } else if (uboMatricesAndUserInput.mVertexColorMode == 2) { // based on length
        float factor = distance(gl_in[1].gl_Position, gl_in[2].gl_Position) / uboMatricesAndUserInput.mDataMaxLineLength;
        colA.rgb = mix(uboMatricesAndUserInput.mVertexColorMin.rgb, uboMatricesAndUserInput.mVertexColorMax.rgb, factor);
        colB.rgb = colA.rgb;
    } else if (uboMatricesAndUserInput.mVertexColorMode == 3) { // per Polyline
        colA.rgb = color_from_id_hash(pushConstants.drawCallIndex);
        colB.rgb = colA.rgb;
    } else if (uboMatricesAndUserInput.mVertexColorMode == 4) { // per Line
        colA.rgb = color_from_id_hash(gl_PrimitiveIDIn);
        colB.rgb = colA.rgb;
    }

    if (uboMatricesAndUserInput.mVertexAlphaMode == 0) {
        colA.a = uboMatricesAndUserInput.mVertexAlphaBounds.x;
        colB.a = uboMatricesAndUserInput.mVertexAlphaBounds.x;
    } else if (uboMatricesAndUserInput.mVertexAlphaMode == 1) {  // based on velocity
        colA.a = mix(uboMatricesAndUserInput.mVertexAlphaBounds.x, uboMatricesAndUserInput.mVertexAlphaBounds.y, uboMatricesAndUserInput.mVertexAlphaInvert ? 1 - inRadius[1] : inRadius[1]);
        colB.a = mix(uboMatricesAndUserInput.mVertexAlphaBounds.x, uboMatricesAndUserInput.mVertexAlphaBounds.y, uboMatricesAndUserInput.mVertexAlphaInvert ? 1 - inRadius[2] : inRadius[2]);
    } else if (uboMatricesAndUserInput.mVertexAlphaMode == 2) { // based on length
        float factor = distance(gl_in[1].gl_Position, gl_in[2].gl_Position) / uboMatricesAndUserInput.mDataMaxLineLength;
        factor = uboMatricesAndUserInput.mVertexAlphaInvert ? 1 - factor : factor;
        colA.a = mix(uboMatricesAndUserInput.mVertexAlphaBounds.x, uboMatricesAndUserInput.mVertexAlphaBounds.y, inRadius[1]);
        colB.a = colA.a;
    }
    
    if (uboMatricesAndUserInput.mVertexRadiusMode == 0) {
        radA = uboMatricesAndUserInput.mVertexRadiusBounds.x;
        radB = uboMatricesAndUserInput.mVertexRadiusBounds.x;
    } else if (uboMatricesAndUserInput.mVertexRadiusMode == 1) {  // based on velocity
        radA = mix(uboMatricesAndUserInput.mVertexRadiusBounds.x, uboMatricesAndUserInput.mVertexRadiusBounds.y, uboMatricesAndUserInput.mVertexRadiusInvert ? 1 - inRadius[1] : inRadius[1]);
        radB = mix(uboMatricesAndUserInput.mVertexRadiusBounds.x, uboMatricesAndUserInput.mVertexRadiusBounds.y, uboMatricesAndUserInput.mVertexRadiusInvert ? 1 - inRadius[2] : inRadius[2]);
    } else if (uboMatricesAndUserInput.mVertexRadiusMode == 2) { // based on length
        float factor = distance(gl_in[1].gl_Position, gl_in[2].gl_Position) / uboMatricesAndUserInput.mDataMaxLineLength;
        factor = uboMatricesAndUserInput.mVertexRadiusInvert ? 1 - factor : factor;
        radA = mix(uboMatricesAndUserInput.mVertexRadiusBounds.x, uboMatricesAndUserInput.mVertexRadiusBounds.y, factor);
        radA = radB;
    }



    construct_billboard_for_line(
        gl_in[1].gl_Position, gl_in[2].gl_Position,
        radA, radB,
        uboMatricesAndUserInput.mCamPos, uboMatricesAndUserInput.mCamDir,
        gl_in[0].gl_Position, gl_in[3].gl_Position,
        colA, colB
    );
}