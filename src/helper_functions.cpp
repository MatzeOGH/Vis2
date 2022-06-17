#include "helper_functions.h"


glm::vec4 uIntTo4Col(unsigned int val) {
	// Jeweils 8 bit für eine Komponente
	glm::vec4 tmp(0.0f);
	tmp.r = (float)((val & 0xFF000000) >> (8 * 3)) / 255.0f;
	tmp.g = (float)((val & 0x00FF0000) >> (8 * 2)) / 255.0f;
	tmp.b = (float)((val & 0x0000FF00) >> (8 * 1)) / 255.0f;
	tmp.a = (float)((val & 0x000000FF) >> (8 * 0)) / 255.0f;
	return tmp;
}

unsigned int getDigitCountForUInt(unsigned int src)
{
	unsigned int cnt = 1;
	while (src > 9) {
		src /= 10;
		cnt++;
	}
	return cnt;
}
