#pragma once
#include "BSP_util.h"

namespace BSPGUY
{

class Keyvalue
{
public:
	std::string key;
	std::string value;

	Keyvalue(std::string line);
	Keyvalue(std::string key, std::string value);
	Keyvalue(void);
	~Keyvalue(void);

	vec3 getVector();
};

} // namespace BSPGUY

