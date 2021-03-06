#include "Dataset.h"

#include <filesystem>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <iostream>
#include <fast_float/fast_float.h>

#include "helper_functions.h"

enum class eStatus {
	INITIAL,
	AFTERV,
	READV,
	READVT,
	READL, // This status is obsolete because of the hack mentioned later
	IGNOREUNTILNEWLINE
};

Dataset::Dataset()
{
	initializeValues();
}

void Dataset::importFromFile(std::string filename)
{
	initializeValues();
	auto start = std::chrono::steady_clock::now();

	// check if file ends with .obj and if it exists
	const std::filesystem::path filePath{ filename };
	const std::filesystem::path obj_extension{ ".obj" };
	if (filePath.extension() != obj_extension) throw std::runtime_error("extension error: " + filePath.extension().string());
	if (!std::filesystem::exists(filePath)) throw std::runtime_error("file " + filePath.filename().string() + " does not exist");
	mName = filePath.filename().string();

	// open ifstream for file
	std::ifstream inFile{ filePath, std::ios::in | std::ios::binary };
	if (!inFile) throw std::runtime_error("failed to load file " + filePath.filename().string());

	// copy file into data
	std::string data(static_cast<size_t>(std::filesystem::file_size(filePath)), 0);
	inFile.read(data.data(), data.size());

	std::vector<Vertex> tmpVertexBuffer;
	std::vector<Poly> newPolyBuffer;
	Vertex tmpVertex;
	unsigned int currVertexCountInPolyBuffer = 0;

	eStatus status = eStatus::INITIAL;
	int curLine = 1;
	char tmpNumber[20];	// Note: This array is not even really necessary and can be replaced by Start- and End-indexes refering to data.
	int tNl = 0;
	glm::vec3 tmpPosition = glm::vec3(0.0F); int tPi = 0;

	for (std::string::size_type i = 0; i <= data.size(); ++i) {
		char& c = data[i];

		if (c == '\r') continue;

		switch (status) {

		case eStatus::INITIAL:
			if (c == 'v') status = eStatus::AFTERV;
			else if (c == 'l') {
				// INFO: The following is some kind of a hack: In all provided datasets the indices given for the lines
				// are just exactly made out of the vertices before in linear order. Therefore we don't care about the actual index numbers and
				// whenever we encounter an l line we just assume that. It saves tons of interpreting work...
				
				// The following works since the size of newLineBuffer is similar to the current index number and should speed up the process even more
				// as we don't have to walk through the unnecessary length of the indices
				unsigned int jumpForward = (getDigitCountForUInt(currVertexCountInPolyBuffer) + 1) * tmpVertexBuffer.size(); 
				currVertexCountInPolyBuffer += tmpVertexBuffer.size();
				newPolyBuffer.push_back({ std::move(tmpVertexBuffer) });
				if (data[i+1] != '\r' && data[i+1] != '\0') i += jumpForward;	// Only jump forward if there is no immediate line break
				status = eStatus::IGNOREUNTILNEWLINE;
			}
			else if (c == 'g') status = eStatus::IGNOREUNTILNEWLINE;
			break;

		case eStatus::AFTERV:
			if (c == 't') status = eStatus::READVT;
			else if (c == ' ') status = eStatus::READV;
			else status = eStatus::INITIAL; // shouldnt happen (go back to INITIAL)
			break;

		case eStatus::READVT:
			if (c == '\n' || c == ' ') {
				if (tNl > 0) { // if not its probably just an extra line indent
					// Number should now be in tmpNumber
					float result;
					auto answer = fast_float::from_chars(&tmpNumber[0], &tmpNumber[tNl], result);
					if (answer.ec != std::errc()) {
						tmpNumber[tNl] = '\0';
						std::cerr << "float parsing failure of vt-string '" << tmpNumber << "' in line " << curLine << std::endl;
					}
					else {
						tmpVertex.data = result;
						// vertex info is now complete. Add to vertexbuffer:
						tmpVertexBuffer.push_back(tmpVertex);
						status = eStatus::INITIAL;
					}
				}
				tNl = 0;
			}
			else {
				tmpNumber[tNl] = c;
				tNl++;
			}
			break;

		case eStatus::READV:
			if (c == '\n' || c == ' ') {
				if (tNl > 0) { // if not its probably just an extra line indent
					float result;
					auto answer = fast_float::from_chars(&tmpNumber[0], &tmpNumber[tNl], result);
					if (answer.ec != std::errc()) {
						tmpNumber[tNl] = '\0';
						std::cerr << "float parsing failure of v-string '" << tmpNumber << "' for component " << tPi << " in line " << curLine << std::endl;
					}
					else {
						tmpPosition[tPi] = result;
						tPi++;
						if (tPi == 3) {
							// We gathered three floats:
							tmpVertex.position = tmpPosition;
							tPi = 0;
							status = eStatus::INITIAL;
						}
					}
				}
				if (c == '\n') {
					status = eStatus::INITIAL;
				}
				tNl = 0;
			}
			else {
				tmpNumber[tNl] = c;
				tNl++;
			}
			break;

		case eStatus::IGNOREUNTILNEWLINE:
			if (c == '\n') status = eStatus::INITIAL;
			break;
		}

		if (c == '\n') curLine++;
	}

	this->mLastLoadingTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count() * 0.000000001;

	mPolyLineBuffer = std::move(newPolyBuffer);

	this->preprocessLineData();

}


void Dataset::fillGPUReadyBuffer(std::vector<Vertex>& newVertexBuffer, std::vector<uint32_t>& newIndexBuffer)
{
	newVertexBuffer.clear();
	newIndexBuffer.clear();
	newVertexBuffer.reserve(mVertexCount);
	newIndexBuffer.reserve(mVertexCount);
	uint32_t currIndex = 0;
	for (Poly& pl : mPolyLineBuffer) {
		for (uint32_t i = 0; i < pl.vertices.size() - 1; i++) {
			newVertexBuffer.push_back(pl.vertices[i]);
			newVertexBuffer.push_back(pl.vertices[i + 1]);
			newIndexBuffer.push_back(currIndex++);
			newIndexBuffer.push_back(currIndex++);
		}
	}
}

void Dataset::fillGPUReadyBuffer(std::vector<Vertex>& newVertexBuffer, std::vector<draw_call_t>& newDrawCalls) {
	newVertexBuffer.clear();
	newDrawCalls.clear();
	newVertexBuffer.reserve(mVertexCount);
	newDrawCalls.reserve(mPolyLineBuffer.size());
	unsigned int currIndex = 0;
	for (Poly& pl : mPolyLineBuffer) {
		newVertexBuffer.insert(newVertexBuffer.end(), pl.vertices.begin(), pl.vertices.end());
		newDrawCalls.push_back({ (uint32_t)currIndex, (uint32_t)pl.vertices.size() });
		currIndex += pl.vertices.size();
	}
}

void Dataset::preprocessLineData()
{
	auto start = std::chrono::steady_clock::now();
	
	auto boundsCurvature = glm::vec2(100.0F, 0.0F);
	mLineCount = 0;
	mVertexCount = 0;
	for (Poly& pLine : mPolyLineBuffer) {
		float lastLineLength = 0.0F;
		for (unsigned int i = 0; i < pLine.vertices.size(); i++) {
			Vertex& v = pLine.vertices[i];
			v.position = glm::clamp(v.position, 0.0F, 1.0F);
			v.data = glm::clamp(v.data, 0.0F, 1.0F);

			mMaximumCoordinateBounds = glm::max(v.position, mMaximumCoordinateBounds);
			mMinimumCoordinateBounds = glm::min(v.position, mMinimumCoordinateBounds);
			mMinDataValue = glm::min(v.data, mMinDataValue);
			mMaxDataValue = glm::max(v.data, mMaxDataValue);
			
			float preLength = 0.0F;
			float postLength = 0.0F;
			if (i > 0) preLength = glm::distance(pLine.vertices[i - 1].position, pLine.vertices[i].position);
			if (i < pLine.vertices.size() - 1) postLength = glm::distance(pLine.vertices[i].position, pLine.vertices[i + 1].position);
			mMaxLineLength = glm::max(mMaxLineLength, preLength);
			mMaxVertexAdjacentLineLength = glm::max(mMaxVertexAdjacentLineLength, preLength + postLength);

			if (preLength > 0 && postLength > 0) { //means its an in between point and not degenerate so calculate curvature
				glm::vec3 a = pLine.vertices[i + 1].position - pLine.vertices[i].position;
				glm::vec3 b = pLine.vertices[i - 1].position - pLine.vertices[i].position;
				float alpha = glm::acos(glm::dot(a, b) / (preLength * postLength));
				boundsCurvature[0] = glm::min(alpha, boundsCurvature[0]);
				boundsCurvature[1] = glm::max(alpha, boundsCurvature[1]);
				pLine.vertices[i].curvature = alpha;
			}
		}
		// Mark beginnings and ends
		pLine.vertices[0].position.x *= -1;
		pLine.vertices[pLine.vertices.size() - 1].position.x *= -1;

		mLineCount += pLine.vertices.size() / 2 - 1;
		mVertexCount += pLine.vertices.size();
	}

	// Now lets run again and scale the data and curvature in between 0 and 1
	for (Poly& pLine : mPolyLineBuffer) {
		for (Vertex& v : pLine.vertices) {
			v.data = (v.data - mMinDataValue) / (mMaxDataValue - mMinDataValue);
			// Note: The following is not the best solution. Degenerate points(lines) should maybe get the curvature
			// of the next or previous one. But in our datasets those dont happen anyway
			if (v.curvature < 0.0) v.curvature = boundsCurvature[0]; 
			v.curvature = 1.0F - (v.curvature - boundsCurvature[0]) / (boundsCurvature[1] - boundsCurvature[0]);
		}
	}

	this->mLastPreprocessTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count() * 0.000000001;
}

void Dataset::initializeValues()
{
	mLastLoadingTime = 0.0F;
	mLastPreprocessTime = 0.0F; 
	mMinimumCoordinateBounds = glm::vec3(1.0F, 1.0F, 1.0F);
	mMaximumCoordinateBounds = glm::vec3(0.0F, 0.0F, 0.0F);
	mMinDataValue = 1.0F;
	mMaxDataValue = 0.0F;
	mLineCount = 0;
	mVertexCount = 0;
	mMaxLineLength = 0.0F;
	mMaxVertexAdjacentLineLength = 0.0F;
}
