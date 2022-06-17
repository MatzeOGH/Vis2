#pragma once

#include <glm/glm.hpp>

glm::vec4 uIntTo4Col(unsigned int val);

/// <summary>
/// Returns the count of digits of the given unsined int.
/// Example: getDigitCountForUInt(21349) = 5
/// </summary>
unsigned int getDigitCountForUInt(unsigned int src);
