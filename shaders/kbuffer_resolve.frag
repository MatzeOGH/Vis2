#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };
layout (set = 1, binding = 0) buffer readonly KBuffer{ uint64_t data[]; } kBuffer;

layout (early_fragment_tests) in;
layout (location = 0) in VertexData
{
	vec2 texCoords;   // texture coordinates
} fs_in;

layout(location = 0) out vec4 outColor;

uint listPos(uint i)
{
    ivec2 coord = ivec2( gl_FragCoord.xy );
    ivec3 imgSize = ivec3(uboMatricesAndUserInput.kBufferInfo.xyz);
    return coord.x + coord.y * imgSize.x + i * (imgSize.x * imgSize.y);
}

const uint K_MAX = 16; 

struct Samples{
	vec4 color;
	float depth;
};

void main()
{
	ivec2 coord = ivec2( gl_FragCoord.xy );

	
	// load k samples from the data buffers
	Samples samples[K_MAX];
	uint sample_count = 0;
	for(int i = 0; i < int(uboMatricesAndUserInput.kBufferInfo.z); ++i)
	{
		uint64_t value = kBuffer.data[listPos(i)];
		if(value == packUint2x32(uvec2(0xFFFFFFFFu, 0xFFFFFFFFu)))
        {
			break;
        }
		const uvec2 unpacked = unpackUint2x32(value);
		samples[i].color = unpackUnorm4x8(unpacked.x);
		samples[i].depth = uintBitsToFloat(unpacked.y);
		sample_count++;
	}

	if(sample_count == 0)
		discard;


	vec3 color = vec3(0);
	float alpha = 1;
	for(uint i = 0; i < sample_count; ++i)
	{
		color = color * (1 -samples[sample_count-i-1].color.a) + samples[sample_count-i-1].color.rgb;
		alpha *= samples[sample_count-i-1].color.a;
	}

	alpha = 1 - alpha;
	outColor = vec4(color / alpha, alpha);
}
