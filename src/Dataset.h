#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

#include "host_structures.h"

/// <summary>
/// This class represents a Line-Dataset. It furthermore offers functionality to load data of an .obj-file and
/// does some preprocessing-steps.
/// </summary>
class Dataset
{

public:
	Dataset();

	void importFromFile(std::string filename);

	/// <summary>
	/// This function fills a vertex-list and an index list. the vertices at in between points of a line
	/// get duplicated as proposed in the paper.
	/// </summary>
	void fillGPUReadyBuffer(std::vector<Vertex>& newVertexBuffer, std::vector<uint32_t>& newIndexBuffer);

	/// <summary>
	/// This function fills a vertex-list and a list with information for the draw calls of the different
	/// line segments.
	/// </summary>
	void fillGPUReadyBuffer(std::vector<Vertex>& newVertexBuffer, std::vector<draw_call_t>& newDrawCalls);

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

	float getMaxLineLength() { return this->mMaxLineLength; }

	float getMaxVertexAdjacentLineLength() { return this->mMaxVertexAdjacentLineLength; }

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
	
	float mMaxLineLength = 0.0F;
	float mMaxVertexAdjacentLineLength = 0.0F;

	/// <summary>
	/// This function gathers information about the dataset which is later used for the dynamic
	/// transformations. It furthermore edits the x coordinate of the end- and start points for correct
	/// cap clipping. It furthermore transforms scales the velocity in between 0 and 1
	/// </summary>
	void preprocessLineData();

	/// <summary>
	/// Initializes the member variables with their initial values.
	/// </summary>
	void initializeValues();

};

