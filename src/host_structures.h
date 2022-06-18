#pragma once

#include <vulkan/vulkan_core.h>

/// <summary>
/// Represents one point of a line
/// </summary>
struct Vertex {

	/// <summary>
	/// The position of the point
	/// </summary>
	glm::vec3 pos = { -1.0F, -1.0F, -1.0F };

	/// <summary>
	/// The radius/velocity of this point
	/// </summary>
	float radius = -1.0F;
};

/// <summary>
/// Represents a connected group of lines
/// </summary>
struct Poly {

	/// <summary>
	/// The vertices that make up this Polyline
	/// </summary>
	std::vector<Vertex> vertices;

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

	glm::vec4 mVertexColorMin;
	glm::vec4 mVertexColorMax;
	glm::vec4 mVertexAlphaBounds;
	glm::vec4 mVertexRadiusBounds;

	VkBool32 mBillboardClippingEnabled;
	VkBool32 mBillboardShadingEnabled;

	uint32_t mVertexColorMode;
	uint32_t mVertexAlphaMode;
	uint32_t mVertexRadiusMode;

	VkBool32 mVertexAlphaInvert;
	VkBool32 mVertexRadiusInvert;
	float mDataMaxLineLength;
	float mDataMaxVertexAdjacentLineLength;

};
