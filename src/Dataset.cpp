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

void Dataset::importFromFile(std::string filename)
{
	auto start = std::chrono::steady_clock::now();

	// check if file ends with .obj and if it exists
	const std::filesystem::path filePath{ filename };
	const std::filesystem::path obj_extension{ ".obj" };
	if (filePath.extension() != obj_extension) throw std::runtime_error("extension error: " + filePath.extension().string());
	if (!std::filesystem::exists(filePath)) throw std::runtime_error("file " + filePath.filename().string() + " does not exist");

	// open ifstream for file
	std::ifstream inFile{ filePath, std::ios::in | std::ios::binary };
	if (!inFile) throw std::runtime_error("failed to load file " + filePath.filename().string());

	// copy file into data
	std::string data(static_cast<size_t>(std::filesystem::file_size(filePath)), 0);
	inFile.read(data.data(), data.size());

	std::vector<Vertex> tmpVertexBuffer;
	std::vector<Line> mNewLineBuffer;
	Vertex tmpVertex;
	int currObjectId = 0;

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
				unsigned int jumpForward = (getDigitCountForUInt(mNewLineBuffer.size()) + 1) * tmpVertexBuffer.size(); 
				for (unsigned int i2 = 0; i2 < tmpVertexBuffer.size() - 1; i2++) {
					mNewLineBuffer.push_back({ currObjectId, tmpVertexBuffer[i2], tmpVertexBuffer[i2 + 1u] });
				}
				tmpVertexBuffer.clear();
				currObjectId++;
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
						tmpVertex.radius = result;
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
							tmpVertex.pos = tmpPosition;
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

	mLineBuffer = std::move(mNewLineBuffer);

}


void Dataset::fillGPUReadyBuffer(std::vector<Vertex>& newVertexBuffer, std::vector<uint32_t>& newIndexBuffer)
{
	newVertexBuffer.clear();
	newIndexBuffer.clear();
	newVertexBuffer.reserve(mLineBuffer.size() * 2);
	newIndexBuffer.reserve(mLineBuffer.size() * 2);
	uint32_t currIndex = 0;
	for (Line& l : mLineBuffer) {
		newVertexBuffer.push_back(l.vFrom);
		newVertexBuffer.push_back(l.vTo);
		newIndexBuffer.push_back(currIndex++);
		newIndexBuffer.push_back(currIndex++);
	}
}

void Dataset::preprocessLineData()
{
	auto start = std::chrono::steady_clock::now();

	int lastObjectId = -1;
	mPolylineCount = 0;
	for (unsigned int i = 0; i < mLineBuffer.size(); i++) {
		Line& l = mLineBuffer[i];
		// We only support positions in the positive octant and inside the unitcube
		l.vFrom.pos = glm::clamp(l.vFrom.pos, 0.0F, 1.0F);
		l.vTo.pos = glm::clamp(l.vTo.pos, 0.0F, 1.0F);
		l.vFrom.radius = glm::clamp(l.vFrom.radius, 0.0F, 1.0F);
		l.vTo.radius = glm::clamp(l.vTo.radius, 0.0F, 1.0F);

		mMaximumCoordinateBounds = glm::max(l.vTo.pos, mMaximumCoordinateBounds);
		mMinimumCoordinateBounds = glm::min(l.vTo.pos, mMinimumCoordinateBounds);
		mMinVelocity = glm::min(l.vTo.radius, mMinVelocity);
		mMaxVelocity = glm::max(l.vTo.radius, mMaxVelocity);

		if (lastObjectId != l.objectId) {
			// Start-caps appear just once so make sure we check it for its bounds
			mMaximumCoordinateBounds = glm::max(l.vFrom.pos, mMaximumCoordinateBounds);
			mMinimumCoordinateBounds = glm::min(l.vFrom.pos, mMinimumCoordinateBounds);
			mMinVelocity = glm::min(l.vFrom.radius, mMinVelocity);
			mMaxVelocity = glm::max(l.vFrom.radius, mMaxVelocity);
			// Its a new object so we have to inverse the x axis of the last and this point:
			if (i > 0) mLineBuffer[i].vTo.pos.x *= -1.0; // This was an end-cap
			l.vFrom.pos.x *= -1.0; // This is a start-cap
			mPolylineCount++;
		}
		lastObjectId = l.objectId;
	}

	this->mLastPreprocessTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count() * 0.000000001;
}
