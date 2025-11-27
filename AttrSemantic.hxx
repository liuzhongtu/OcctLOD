// AttrSemantic.hxx
#pragma once

#include <string>

enum class AttrSemantic
{
	Position,       // XYZ
	Normal,         // NX NY NZ
	Color,          // RGB
	Intensity,      // 激光强度
	Classification, // 点分类标签
	User0,
	User1,
	User2,
};