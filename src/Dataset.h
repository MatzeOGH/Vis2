#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

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

/// <summary>
/// This class represents a Line-Dataset. It furthermore offers functionality to load data of an .obj-file and
/// does some preprocessing-steps.
/// </summary>
class Dataset
{

public:
	void importFromFile(std::string filename);

	/// <summary>
	/// This function fills a vertex-list and an index list. the vertices at in between points of a line
	/// get duplicated as proposed in the paper.
	/// </summary>
	void fillGPUReadyBuffer(std::vector<Vertex>& newVertexBuffer, std::vector<uint32_t>& newIndexBuffer);

	/// <summary>
	/// Returns the time it took to import the last file in seconds
	/// </summary>
	float getLastLoadingTime() { return mLastLoadingTime; }

	/// <summary>
	/// Returns the time it took to preprocess the last file in seconds
	/// </summary>
	float getLastPreprocessTime() { return mLastPreprocessTime; }

	/// <summary>
	/// Returns the amount of line individual line segments inside the file
	/// </summary>
	int getLineCount() { return this->mLineCount; }

	/// <summary>
	/// Returns the amount of connected line segments
	/// </summary>
	int getPolylineCount() { return this->mPolyLineBuffer.size(); }

	/// <summary>
	/// Returns the amount of singular vertices in the dataset
	/// </summary>
	int getVertexCount() { return this->mVertexCount; }

private:
	std::vector<Poly> mPolyLineBuffer; // contains the loaded polylines
	float mLastLoadingTime = 0.0F; // stores the time it took to load the data into cpu memory
	float mLastPreprocessTime = 0.0F; // stores the time it took to preprocess the current data in seconds

	glm::vec3 mMinimumCoordinateBounds = glm::vec3(1.0F, 1.0F, 1.0F);
	glm::vec3 mMaximumCoordinateBounds = glm::vec3(0.0F, 0.0F, 0.0F);
	float mMinVelocity = 1.0F;
	float mMaxVelocity = 0.0F;

	int mLineCount = 0;
	int mVertexCount = 0;

	/// <summary>
	/// This function gathers information about the dataset which is later used for the dynamic
	/// transformations. It furthermore edits the x coordinate of the end- and start points for correct
	/// cap clipping.
	/// </summary>
	void preprocessLineData();

};

