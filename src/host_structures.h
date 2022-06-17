#pragma once

struct Vertex {
	glm::vec3 pos;
	glm::vec4 color;
	float radius;
};

struct draw_call_t
{
	uint32_t firstIndex;
	uint32_t numberOfPrimitives;
};

struct matrices_and_user_input {
	glm::mat4 mViewMatrix;
	glm::mat4 mProjMatrix;
	glm::vec4 mCamPos;
	glm::vec4 mCamDir;
	glm::vec4 mClearColor;
	glm::vec4 mHelperLineColor;
	glm::vec4 mkBufferInfo;


	glm::vec4 mDirLightDirection; // normalize(vec3(-0.7, -0.6, -0.3));
	glm::vec4 mDirLightColor; // vec3(1.0, 1.0, 1.0);
	glm::vec4 mAmbLightColor; // vec3(0.05, 0.05, 0.05);
	glm::vec4 mMaterialLightReponse; // vec4(0.5, 1.0, 0.5, 32.0);  // amb, diff, spec, shininess

	VkBool32 mUseVertexColorForHelperLines;
	VkBool32 mBillboardClippingEnabled;
	int mNumberOfLines;
};

struct downsample
{
	glm::vec2 mOutputSize;
	glm::vec2 mInvInputSize;
};