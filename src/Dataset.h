#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

struct Vertex {
	glm::vec3 pos = { -1.0F, -1.0F, -1.0F };
	//glm::vec4 color = { 0.5f, 0.5f, 0.5f, 1.0f };
	float radius = -1.0F;
};

struct Line {

	/// <summary>
	/// The unique id of the polyline this line is part of. (Could be useful later)
	/// </summary>
	int objectId;

	/// <summary>
	/// The vertex-object this line originates from
	/// </summary>
	Vertex vFrom;

	/// <summary>
	/// The vertex-object this line ends up at
	/// </summary>
	Vertex vTo;
};

class Dataset
{

public:
	void importFromFile(std::string filename);

	void fillGPUReadyBuffer(std::vector<Vertex>& newVertexBuffer, std::vector<uint32_t>& newIndexBuffer);

	/// <summary>
	/// Returns the time it took to import the last file
	/// </summary>
	float getLastLoadingTime() { return mLastLoadingTime; }

	float getLastPreprocessTime() { return mLastPreprocessTime; }

	int getLineCount() { return this->mLineBuffer.size(); }

	int getPolylineCount() { return this->mPolylineCount; }

private:
	std::vector<Line> mLineBuffer;
	float mLastLoadingTime = 0.0F;
	float mLastPreprocessTime = 0.0F;

	glm::vec3 mMinimumCoordinateBounds = glm::vec3(1.0F, 1.0F, 1.0F);
	glm::vec3 mMaximumCoordinateBounds = glm::vec3(0.0F, 0.0F, 0.0F);
	float mMinVelocity = 1.0F;
	float mMaxVelocity = 0.0F;

	int mPolylineCount = 0;

	/// <summary>
	/// This function gathers information about the dataset which is later used for the dynamic
	/// transformations. It furthermore edits the x coordinate of the end- and start points for correct
	/// cap clipping.
	/// </summary>
	void preprocessLineData();

};

