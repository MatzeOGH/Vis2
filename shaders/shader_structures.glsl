/**
 * Contains the ubo struct for variables. Gets included by most shaders
 * @author Gerald Kimmersdorfer, Mathias Hürbe
 * @date 2022
 */

#ifndef SHADER_STRUCTURES_GLSL
// ###### UNIFORMS AND PUSH CONSTANTS ###############
// Uniform buffer struct, containing camera matrices and user input:
// It is updated every frame. For more information about the member Variables 
// have a look at host_structures.h
struct matrices_and_user_input {
	mat4 mViewMatrix;
	mat4 mProjMatrix;
	vec4 mCamPos;
	vec4 mCamDir;
	vec4 mClearColor;
	vec4 mHelperLineColor;
	vec4 kBufferInfo; 

	vec4 mDirLightDirection;
    vec4 mDirLightColor;
    vec4 mAmbLightColor;
    vec4 mMaterialLightReponse;

	vec4 mVertexColorMin;
	vec4 mVertexColorMax;
	vec4 mVertexAlphaBounds;
	vec4 mVertexRadiusBounds;
	
	bool mBillboardClippingEnabled;
	bool mBillboardShadingEnabled;
	uint mVertexColorMode;
	uint mVertexAlphaMode;
	uint mVertexRadiusMode;

	bool mVertexAlphaInvert;
	bool mVertexRadiusInvert;
	float mDataMaxLineLength;
	float mDataMaxVertexAdjacentLineLength;
};

#define SHADER_STRUCTURES_GLSL 1
#endif

