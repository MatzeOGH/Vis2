#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 inPosA;
layout(location = 3) in vec3 inPosB;
layout(location = 4) in vec2 inRARB;

layout(location = 0) out vec4 outColor;


// Inigo Quilez https://www.shadertoy.com/view/MlKfzm
vec4 iRoundedCone( in vec3  ro, in vec3  rd, 
                   in vec3  pa, in vec3  pb, 
                   in float ra, in float rb )
{
    vec3  ba = pb - pa;
	vec3  oa = ro - pa;
	vec3  ob = ro - pb;
    float rr = ra - rb;
    float m0 = dot(ba,ba);
    float m1 = dot(ba,oa);
    float m2 = dot(ba,rd);
    float m3 = dot(rd,oa);
    float m5 = dot(oa,oa);
	float m6 = dot(ob,rd);
    float m7 = dot(ob,ob);
    
    float d2 = m0-rr*rr;
    
	float k2 = d2    - m2*m2;
    float k1 = d2*m3 - m1*m2 + m2*rr*ra;
    float k0 = d2*m5 - m1*m1 + m1*rr*ra*2.0 - m0*ra*ra;
    
	float h = k1*k1 - k0*k2;
	if(h < 0.0) return vec4(-1.0);
    float t = (-sqrt(h)-k1)/k2;
  //if( t<0.0 ) return vec4(-1.0);

    float y = m1 - ra*rr + t*m2;
    if( y>0.0 && y<d2 ) 
    {
        return vec4(t, normalize( d2*(oa + t*rd)-ba*y) );
    }

    // Caps. I feel this can be done with a single square root instead of two
    float h1 = m3*m3 - m5 + ra*ra;
    float h2 = m6*m6 - m7 + rb*rb;
    if( max(h1,h2)<0.0 ) return vec4(-1.0);
    
    vec4 r = vec4(1e20);
    if( h1>0.0 )
    {        
    	t = -m3 - sqrt( h1 );
        r = vec4( t, (oa+t*rd)/ra );
    }
	if( h2>0.0 )
    {
    	t = -m6 - sqrt( h2 );
        if( t<r.x )
        r = vec4( t, (ob+t*rd)/rb );
    }
    
    return r;
}

void main() {

    outColor = vec4(0);

    vec3 camWS = uboMatricesAndUserInput.mCamPos.xyz;
    vec3 viewRayWS = normalize(fragColor.xyz);

    vec4 tnor = iRoundedCone(camWS, viewRayWS, inPosA, inPosB, inRARB.x, inRARB.y);
    float t = tnor.x;
    if(t <= 0.0)
    {
        discard;
    }

    vec3 lig = normalize(vec3(0.7,0.6,0.3));
    vec3 hal = normalize(-viewRayWS+lig);
    vec3 nor = tnor.yzw;
    float dif = clamp( dot(nor,lig), 0.0, 1.0 );

    outColor = vec4( vec3(1.0,0.9,0.7)*dif + vec3(0.01), 0.3);
    //outColor = vec4(nor, 0.5);

}