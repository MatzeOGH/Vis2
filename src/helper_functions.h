#pragma once

#include <glm/glm.hpp>
#include <string>

/// <summary>
/// Needs to be called after the setup of the dear gui and changes the color theme.
/// WARNING: lightMode is buggy!
/// </summary>
/// <param name="darkMode">dark/light mode flag</param>
/// <param name="alpha">The overall alpha level of the theme</param>
void activateImGuiStyle(bool darkMode = true, float alpha = 0.2F);

/// <summary>
/// Returns the count of digits of the given unsined int.
/// Example: getDigitCountForUInt(21349) = 5
/// </summary>
unsigned int getDigitCountForUInt(unsigned int src);
