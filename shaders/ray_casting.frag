#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

// kbuffer storage 
layout (set = 1, binding = 0, r32ui) volatile coherent uniform uimage2D semaphore;
layout (set = 1, binding = 1, r8ui) coherent uniform uimage2D count;
layout (set = 1, binding = 2, r32f) coherent uniform image2DArray depths;
layout (set = 1, binding = 3, rgba16f) coherent uniform image2DArray fragmentSamples;

layout(location = 0) in vec4 inViewRay;
layout(location = 1) in vec4 inFragColor;
layout(location = 2) in vec3 inPosA;
layout(location = 3) in vec3 inPosB;
layout(location = 4) in vec2 inRARB;
layout(location = 5) in vec3 inN0;
layout(location = 6) in vec3 inN1;
layout(location = 7) in vec3 inPosWS;

layout(location = 0) out vec4 outColor;

const uint K_MAX = 8; 


// spin lock buffer
bool acquire_semaphore(ivec2 coord);
void release_semaphore(ivec2 coord);

void insert(ivec2 coord, uint current_count, vec4 color);


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
    ivec2 coord = ivec2( gl_FragCoord.xy );
    uint current_count = imageLoad( count, coord).r;



    vec3 camWS = uboMatricesAndUserInput.mCamPos.xyz;
    vec3 viewRayWS = normalize(inViewRay.xyz);

    vec4 tnor = iRoundedCone(camWS, viewRayWS, inPosA, inPosB, inRARB.x, inRARB.y);
    
    // discard pixel if the rounded cone was not intersected
    float t = tnor.x;
    if(t <= 0.0)
    {
        discard;
    }

    // clip spherical caps
    vec3 n0 = normalize(inPosWS - inPosA);
    vec3 n1 = normalize(inPosWS - inPosB);
    float t0 = dot(n0, inN0);
    float t1 = dot(n1, inN1);
    if(max(t1, t0) > 0)
    {
        //discard;
    }

    // early exit
    bool isFull = current_count >= K_MAX;
    bool isFurther = gl_FragCoord.z > imageLoad(depths, ivec3(coord, 0)).r;
    if(isFull && isFurther)
    {
        discard;
    }

    vec3 lig = normalize(vec3(0.7,0.6,0.3));
    vec3 hal = normalize(-viewRayWS+lig);
    vec3 nor = tnor.yzw;
    float ndotl = clamp( dot(nor,lig), 0.0, 1.0 );

    vec4 color = vec4(inFragColor * ndotl);

    // write fragment to k buffer
    bool done = gl_SampleMaskIn[0] == 0; // ignore helper threads
    while(!done)
    {
        if(imageAtomicCompSwap(semaphore, coord, 0u, 1u) == 0u)
        {
            insert(coord, current_count, color);

            done = true;
            imageAtomicExchange(semaphore, coord, 0u);
        }
    }
}

void kbuffer_store(ivec2 coord, int layer, float depth, vec4 fragment)
{
    ivec3 coordLayer = ivec3(coord, layer);
    imageStore(depths, coordLayer, vec4(depth));
    imageStore(fragmentSamples, coordLayer, fragment);
}

void insert(ivec2 coord, uint current_count, vec4 color)
{
    // k buffer has free slots
    if(current_count < K_MAX)
    {
        // instert into first slot
        if( current_count == 0 )
        {
            kbuffer_store(coord, 0, gl_FragCoord.z, color);
        }
        else
        {
            ivec3 coordLayer = ivec3(coord, 0);

            float oldDepth = imageLoad(depths, coordLayer).r;

            if(gl_FragCoord.z >= oldDepth)
            {
                vec4 oldValue = imageLoad(fragmentSamples, coordLayer);

                // store new fragment at slot 0 
                imageStore(depths, coordLayer, vec4(gl_FragCoord.z));
                imageStore(fragmentSamples, coordLayer, color);
                
                // move old values to next free slot
                coordLayer = ivec3(coord, current_count);
                imageStore(depths, coordLayer, vec4(oldDepth));
                imageStore(fragmentSamples, coordLayer, oldValue);
            }
            else
            {
                coordLayer = ivec3(coord, current_count);
                imageStore(depths, coordLayer, vec4(gl_FragCoord.z));
                imageStore(fragmentSamples, coordLayer, color);
            }
        }
    }
    else
    {
        // find index of furthest depth sample
        int furthestIndex = 0;
        float furthestDepth = -1.0;
        for(int i = 0; i < K_MAX; ++i)
        {
            ivec3 coordLayer = ivec3(coord, i);
            float depth = imageLoad(depths, coordLayer).r;

            if(depth > furthestDepth)
            {
                furthestIndex = i;
                furthestDepth = depth;
            }
        }

        // new sample is the furthest sample
        if(gl_FragCoord.z > furthestDepth)
        {
            kbuffer_store(coord, 0, gl_FragCoord.z, color);
        }
        else
        {
            // move furthest sample to slot 0
            ivec3 coordLayer = ivec3(coord, furthestIndex);
            vec4 oldValue = imageLoad(fragmentSamples, coordLayer);

            kbuffer_store(coord, 0, furthestDepth, oldValue);

            // store current fragment at free slot
            kbuffer_store(coord, furthestIndex, gl_FragCoord.z, color);
        }

    }


    // increse the count
    imageStore( count, ivec2(gl_FragCoord.xy), uvec4(current_count + 1));
}