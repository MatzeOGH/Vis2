
#include "line_importer.h"
#include <string>
#include <string_view>
#include <filesystem>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <iostream>
#include <fast_float/fast_float.h>

#include "glm/glm.hpp"



std::vector<std::string_view> split_string(std::string_view str, char delim)
{
	std::vector<std::string_view> result;

	for (auto first = str.begin(), second = str.begin(), last = str.end();
		second != last && first != last;
		first = std::next(second))
	{
		second = std::find(first, last, delim);

		result.emplace_back(&*first, std::distance(first, second));

		if (second == last) break;
	}

	return result;
}

// returns a none owning list of sting views to the lines 
std::vector<std::string_view> split_lines(std::string_view str)
{
	auto lines = split_string(str, '\n');

	if (!lines.empty() && lines[0].back() == '\r') { // Windows CR conversion
		for (auto&& line : lines) {
			if (line.back() == '\r')
				line.remove_suffix(1);
		}
	}

	return lines;
}

// helper function to convert string_view to float
inline float to_float(std::string_view str)
{
	float result;
	// todo: implement faster conversion
	std::from_chars(str.data(), str.data() + str.size(), result);
	return result;
}

inline int to_int(std::string_view str)
{
	int result;
	std::from_chars(str.data(), str.data() + str.size(), result);
	return result;
}

/// <summary>
/// Load line data from obj
/// </summary>
/// <param name="filename"></param>
void import_file(std::string filename, std::vector<glm::vec3>& position_buffer, std::vector<float>& radius_buffer, std::vector<line_draw_info_t>& line_draw_infos)
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

	//start = std::chrono::steady_clock::now();
	const auto lines = split_lines(data);
	//std::cout << "Time for split lines: " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count() * 0.000000001 << "s" << std::endl;

	//long splitStringTime = 0;
	//long toFloatTime = 0;
	//long lineDrawInfoTime = 0;

	for (auto line : lines)
	{
		//start = std::chrono::steady_clock::now();
		auto data = split_string(line, ' ');
		//splitStringTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

		if (data[0] == "v") // vertex position
		{
			//start = std::chrono::steady_clock::now();
			float x = to_float(data[1]);
			float y = to_float(data[2]);
			float z = to_float(data[3]);
			//toFloatTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
			position_buffer.push_back(glm::vec3(x, y, z));
		}
		else if (data[0] == "vt") // radius
		{
			//start = std::chrono::steady_clock::now();
			float vt = to_float(data[1]);
			//toFloatTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
			radius_buffer.push_back(vt);
		}
		else if (data[0] == "g") // new object
		{

		}
		else if (data[0] == "l") // line
		{
			//start = std::chrono::steady_clock::now();
			line_draw_info_t line_draw_info;
			for (uint32_t idx = 1u; idx < data.size(); ++idx)
			{
				line_draw_info.vertexIds.push_back(to_int(data[idx]) - 1);
			}
			line_draw_infos.push_back(line_draw_info);
			//lineDrawInfoTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
		}
	}

	//std::cout << "Time for split strings: " << splitStringTime * 0.000000001 << "s" << std::endl
	//	<< "Time for toFloat: " << toFloatTime * 0.000000001 << "s" << std::endl
	//	<< "Time for lineDrawInfo: " << lineDrawInfoTime * 0.000000001 << "s" << std::endl;

	std::cout << "Time for import: " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count() * 0.000000001 << "s" << std::endl;

}