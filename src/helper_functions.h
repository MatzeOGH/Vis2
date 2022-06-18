#pragma once

#include <glm/glm.hpp>
#include <string>

glm::vec4 uIntTo4Col(unsigned int val);

void activateImGuiStyle(bool darkMode = true, float alpha = 0.2F);

std::string vec3ToString(glm::vec3 src);

/// <summary>
/// Returns the count of digits of the given unsined int.
/// Example: getDigitCountForUInt(21349) = 5
/// </summary>
unsigned int getDigitCountForUInt(unsigned int src);
