#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

struct line_draw_info_t {
	std::vector<uint32_t> vertexIds;
};

void import_file(std::string filename, std::vector<glm::vec3>& position_buffer, std::vector<float>& radius_buffer, std::vector<line_draw_info_t>& line_draw_infos);