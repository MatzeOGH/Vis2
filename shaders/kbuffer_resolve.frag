#version 460


layout (set = 0, binding = 0, r32ui) uniform readonly restrict uimage2D Count;
layout (set = 0, binding = 1, r32f) uniform readonly restrict  image2DArray Depths;
layout (set = 0, binding = 2, rgba16f) uniform readonly restrict image2DArray fragmentSamples;

layout (location = 0) in VertexData
{
	vec2 texCoords;   // texture coordinates
} fs_in;

layout(location = 0) out vec4 outColor;


const uint K_MAX = 16; 

struct Samples{
	vec4 color;
	float depth;
};

void main()
{
	ivec2 coord = ivec2( gl_FragCoord.xy );

	uint current_count = imageLoad( Count, coord ).r;
	if( current_count == 0) 
	{
		discard;
	}

	// load k samples from the data buffers
	Samples samples[K_MAX];
	for(int i = 0; i < current_count; ++i)
	{
		samples[i].color = imageLoad( fragmentSamples, ivec3(coord, i) );
		samples[i].depth = imageLoad( Depths, ivec3(coord, i)).r;
	}

	// insertion sort sort
	int i = 1;
	while( i < current_count)
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
	float alpha = 0.0;
	for(int i = 0 ; i < current_count; ++i)
	{
		float sampleAlpha = 0.5;
		color = color + samples[i].color.rgb * sampleAlpha * ( 1 - alpha);
		alpha = alpha + sampleAlpha * (1 - alpha);
	}
	//color = samples[0].color.rgb;

	//color = vec3(1.0) - exp(-color * 0.01);
	//color = pow(color, vec3(1.0/2.2));

	outColor = vec4(color, 1);

}

vec3 AlphaBlend( vec3 _FrontColor, vec3 _BackColor, float _Aplha )
{
	return vec3( _FrontColor * _Aplha + _BackColor * ( 1.0 - _Aplha ) );
}