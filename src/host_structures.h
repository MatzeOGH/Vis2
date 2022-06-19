#pragma once

#include <vulkan/vulkan_core.h>

/// <summary>
/// Represents one point of a line
/// </summary>
struct Vertex {

	/// <summary>
	/// The position of the point
	/// </summary>
	glm::vec3 position = { -1.0F, -1.0F, -1.0F };

	/// <summary>
	/// The data (velocity/radius/vorticity) of this point
	/// </summary>
	float data = -1.0F;

	/// <summary>
	/// The curvature of this point (angle between adjacent lines)
	/// The value is mapped in between 0 and 1 by the Dataset-class
	/// </summary>
	float curvature = -1.0F;
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

/// <summary>
/// This structure holds information about when a polyline starts and how long
/// it is inside a sorted Vertex-Buffer
/// </summary>
struct draw_call_t
{
	/// <summary>
	/// The index of the first vertex inside the buffer array
	/// </summary>
	uint32_t firstIndex;
	/// <summary>
	/// The number of vertices that belong to this polyline
	/// </summary>
	uint32_t numberOfPrimitives;
};

/// <summary>
/// A container for the general UBO that we send to the shaders which contains all of the user controllable data
/// </summary>
struct matrices_and_user_input {
	/// <summary>
	/// The view matrix given by the quake cam
	/// </summary>
	glm::mat4 mViewMatrix;
	/// <summary>
	/// The projection matrix given by the quake cam
	/// </summary>
	glm::mat4 mProjMatrix;
	/// <summary>
	/// The position of the camera/eye in world space
	/// </summary>
	glm::vec4 mCamPos;
	/// <summary>
	/// The looking direction of the camera/eye in world space
	/// </summary>
	glm::vec4 mCamDir;
	/// <summary>
	/// rgb ... The background color for the background shader
	/// a   ... The strength of the gradient
	/// </summary>
	glm::vec4 mClearColor;
	/// <summary>
	/// rgb ... The color for the 2d helper lines.
	/// a   ... ignored
	/// </summary>
	glm::vec4 mHelperLineColor;
	/// <summary>
	/// contains resx, resy and kbuffer levels
	/// </summary>
	glm::vec4 mkBufferInfo;

	/// <summary>
	/// The direction of the light/sun in WS
	/// </summary>
	glm::vec4 mDirLightDirection;
	/// <summary>
	/// The color of the light/sun multiplied with the intensity
	/// a   ... ignored
	/// </summary>
	glm::vec4 mDirLightColor;
	/// <summary>
	/// The color of the ambient light
	/// a   ... ignored
	/// </summary>
	glm::vec4 mAmbLightColor;
	/// <summary>
	/// The material light properties for the tubes:
	/// r ... ambient light factor
	/// g ... diffuse light factor
	/// b ... specular light factor
	/// a ... shininess 
	/// </summary>
	glm::vec4 mMaterialLightReponse; // vec4(0.5, 1.0, 0.5, 32.0);  // amb, diff, spec, shininess

	/// <summary>
	/// The vertex color for minimum values (depending on the mode).
	/// Is also used for the color if in static mode
	/// a ... ignored
	/// </summary>
	glm::vec4 mVertexColorMin;
	/// <summary>
	/// The vertex color for vertices with maximum values (depending on the mode)
	/// a ... ignored
	/// </summary>
	glm::vec4 mVertexColorMax;
	/// <summary>
	/// The min/max levels for line transparencies in dynamic modes
	/// The min value is also used if in static mode
	/// ba ... ignored
	/// </summary>
	glm::vec4 mVertexAlphaBounds;
	/// <summary>
	/// The min/max level for the radius of vertices in dynamic modes
	/// The min value is also used if in static mode
	/// ba ... ignored
	/// </summary>
	glm::vec4 mVertexRadiusBounds;

	/// <summary>
	/// Flag to enable/disable the clipping of the billboard based on the raycasting
	/// and for the caps.
	/// </summary>
	VkBool32 mBillboardClippingEnabled;
	/// <summary>
	/// Flag to enable/disable shading of the billboard (raycasting will still be done for possible clipping)
	/// </summary>
	VkBool32 mBillboardShadingEnabled;

	/// <summary>
	/// The coloring mode for vertices (see main->renderUI() for possible states)
	/// </summary>
	uint32_t mVertexColorMode;
	/// <summary>
	/// The transparency mode for vertices (see main->renderUI() for possible states)
	/// </summary>
	uint32_t mVertexAlphaMode;
	/// <summary>
	/// The radius mode for vertices (see main->renderUI() for possible states)
	/// </summary>
	uint32_t mVertexRadiusMode;

	/// <summary>
	/// Reverses the factor (depending on the mode) for dynamic transparency if true
	/// </summary>
	VkBool32 mVertexAlphaInvert;
	/// <summary>
	/// Reverses the factor (depending on the mode) for dynamic radius if true
	/// </summary>
	VkBool32 mVertexRadiusInvert;
	/// <summary>
	/// The maximum line length inside the dataset. Which is necessary to calculate a
	/// factor from 0-1 in the depending on line length mode.
	/// </summary>
	float mDataMaxLineLength;
	/// <summary>
	/// The maximum line length of adjacing lines to a vertex inside the dataset. 
	/// This value is unused so far but could be used for another dynamic mode
	/// </summary>
	float mDataMaxVertexAdjacentLineLength;

};
