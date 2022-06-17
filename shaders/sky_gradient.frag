#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "shader_structures.glsl"


layout (set = 0, binding = 0) uniform UniformBlock { matrices_and_user_input uboMatricesAndUserInput; };

layout (location = 0) out vec4 oFragColor;

void main()
{
	float gradientIntensity = 1 - uboMatricesAndUserInput.mClearColor.a;
	vec3 colorA = mix(uboMatricesAndUserInput.mClearColor.rgb, vec3(0.0), gradientIntensity);
	vec3 colorB = mix(uboMatricesAndUserInput.mClearColor.rgb, vec3(1.0), gradientIntensity);

	vec2 pdc = gl_FragCoord.xy / uboMatricesAndUserInput.kBufferInfo.xy;
	oFragColor = vec4(mix(colorA, colorB, pdc.y), 0.0);
}

