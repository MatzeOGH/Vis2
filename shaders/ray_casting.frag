#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_shader_image_int64 : require
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

// kbuffer storage 
layout (set = 2, binding = 0) buffer coherent KBuffer{ uint64_t data[]; } kBuffer;

layout(location = 0) in vec4 inViewRay;
layout(location = 1) in vec4 inFragColor;
layout(location = 2) in vec3 inPosA;
layout(location = 3) in vec3 inPosB;
layout(location = 4) in vec2 inRARB;
layout(location = 5) in vec3 inN0;
layout(location = 6) in vec3 inN1;
layout(location = 7) in vec3 inPosWS;

layout(location = 0) out vec4 outColor;



// spin lock buffer
bool acquire_semaphore(ivec2 coord);
void release_semaphore(ivec2 coord);

void insert(ivec2 coord, uint current_count, vec4 color);
uint64_t pack(float depth, vec4 color);
void unpack(uint64_t data, out float depth, out vec4 color);

uint listPos(uint i)
{
    ivec2 coord = ivec2( gl_FragCoord.xy );
    ivec3 imgSize = ivec3(uboMatricesAndUserInput.kBufferInfo.xyz);
    return coord.x + coord.y * imgSize.x + i * (imgSize.x * imgSize.y);
}

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

// Returns the distance from the given point to the plane
// Plane must be in Hessian Normal form
float get_distance_from_plane(vec3 point, vec4 plane)
{
    return dot(plane.xyz, point.xyz) - plane.w;
}

// Calculates the diffuse and specular illumination contribution for the given
// parameters according to the Blinn-Phong lighting model.
// All parameters must be normalized.
vec3 calc_blinn_phong_contribution(vec3 toLight, vec3 toEye, vec3 normal, vec3 diffFactor, vec3 specFactor, float specShininess)
{
	float nDotL = max(0.0, dot(normal, toLight)); // lambertian coefficient
	vec3 h = normalize(toLight + toEye);
	float nDotH = max(0.0, dot(normal, h));
	float specPower = pow(nDotH, specShininess);
	vec3 diffuse = diffFactor * nDotL; // component-wise product
	vec3 specular = specFactor * specPower;
	return diffuse + specular;
}

// Calculates the blinn phong illumination for the given fragment
vec3 calculate_illumination(vec3 albedo, vec3 eyePos, vec3 fragPos, vec3 fragNorm) {
    vec4 mMaterialLightReponse = uboMatricesAndUserInput.mMaterialLightReponse;
    vec3 dirColor = uboMatricesAndUserInput.mDirLightColor.rgb;
    vec3 ambient = mMaterialLightReponse.x * inFragColor.rgb;
    vec3 diff = mMaterialLightReponse.y * inFragColor.rgb;
    vec3 spec = mMaterialLightReponse.zzz;
    float shini = mMaterialLightReponse.w;

    vec3 ambientIllumination = ambient * uboMatricesAndUserInput.mAmbLightColor.rgb;
    
    vec3 toLightDirWS = -uboMatricesAndUserInput.mDirLightDirection.xyz;
    vec3 toEyeNrmWS = normalize(eyePos - fragPos);
    vec3 diffAndSpecIllumination = dirColor * calc_blinn_phong_contribution(toLightDirWS, toEyeNrmWS, fragNorm, diff, spec, shini);

    return ambientIllumination + diffAndSpecIllumination;
}

void main() {
    ivec2 coord = ivec2( gl_FragCoord.xy );

    vec3 camWS = uboMatricesAndUserInput.mCamPos.xyz;
    vec3 viewRayWS = normalize(inViewRay.xyz);

    vec4 tnor = iRoundedCone(camWS, viewRayWS, inPosA, inPosB, inRARB.x, inRARB.y);
    vec3 posWsOnCone = camWS + viewRayWS * tnor.x;

    // discard pixel if the rounded cone was not intersected
    if (uboMatricesAndUserInput.mBillboardClippingEnabled) {
        if(tnor.x <= 0.0) discard;
        
        // clip spherical caps
        // get hesse normal forms of planes and calculate distance
        vec4 plane1 = vec4(inN0, dot(inPosA, inN0));
        vec4 plane2 = vec4(inN1, dot(inPosB, inN1));
        float dp1 = get_distance_from_plane(posWsOnCone, plane1);
        float dp2 = get_distance_from_plane(posWsOnCone, plane2);
        if (dp1 > 0 || dp2 > 0) discard;
    }

    vec3 illumination = inFragColor.rgb;
    if (uboMatricesAndUserInput.mBillboardShadingEnabled) {
        illumination = calculate_illumination(inFragColor.rgb, camWS, posWsOnCone, tnor.yzw);
    }
    vec4 color = vec4(illumination * inFragColor.a, 1 - inFragColor.a);

    uint64_t value = pack(gl_FragCoord.z, color);

    //beginInvocationInterlockARB(void);

    bool insert = true;
    if( value > kBuffer.data[listPos(int(uboMatricesAndUserInput.kBufferInfo.z)-1)] )
    {
        insert = false;
    }

    if(insert) {
    for( uint i = 0; i < uint(uboMatricesAndUserInput.kBufferInfo.z); ++i)
    {
        uint64_t old = atomicMin(kBuffer.data[listPos(i)], value);
        if(old == packUint2x32(uvec2(0xFFFFFFFFu, 0xFFFFFFFFu)))
        {
            break;
        }
        value = max(old, value);

        if(i == ((uboMatricesAndUserInput.kBufferInfo.z) - 1))
        {
            if(value != packUint2x32(uvec2(0xFFFFFFFFu, 0xFFFFFFFFu)))
            {
                float depth;
                vec4 color;
                unpack(value, depth, color);

                float alpha = 1-color.a;
                outColor = vec4(color.xyz/alpha, alpha);
            }
            else
            {
                discard;
            }
        }
    }
    }
    //endInvocationInterlockARB(void);


}

uint64_t pack(float depth, vec4 color)
{
    return packUint2x32(uvec2(packUnorm4x8(color), floatBitsToUint(depth)));
}

void unpack(uint64_t data, out float depth, out vec4 color)
{
    const uvec2 unpacked = unpackUint2x32(data);
    color = unpackUnorm4x8(unpacked.x);
    depth = uintBitsToFloat(unpacked.y);
}
