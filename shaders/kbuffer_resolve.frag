#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_shader_image_int64 : require
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"

layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };
layout (set = 1, binding = 0) buffer readonly KBuffer{ uint64_t data[]; } kBuffer;

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

const uint K_MAX = 8; 

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
	for(int i = 0; i < K_MAX; ++i)
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

	// insertion sort sort
	int i = 1;
	while( i < sample_count)
	{
		int j = i;

		while(j > 0 && samples[j-1].depth < samples[j].depth)
		{
			// swap
			Samples temp = samples[j];
			samples[j] = samples[j-1];
			samples[j-1] = temp;
			j--;
		}
		i++;
	}

	vec3 color = vec3(0, 0.0, 0.0);
	for(uint i = 0; i < sample_count; ++i)
	{
		float sampleAlpha = 0.5;
		color = color * (1 - sampleAlpha) + samples[i].color.rgb * sampleAlpha;
	}

	
	//uint64_t value = kBuffer.data[listPos(0)];
	//uvec2 data = unpackUint2x32(value);
	//color = unpackUnorm4x8(data.x).xyz;


	//color = vec3(1.0) - exp(-color * 4);
	//color = pow(color, vec3(1.0/2.2));

	outColor = vec4(color, 1);
}
