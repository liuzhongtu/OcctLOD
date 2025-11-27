// Column.hxx
#pragma once

#include "AttrSemantic.hxx"
#include <cstddef>
#include <algorithm>
#include <Standard_Real.hxx>

// 一个“3维 Standard_Real 列”的只读视图
struct Column3f
{
	AttrSemantic Semantic = AttrSemantic::Position;

	// 指向 SoA 的 X/Y/Z（不拥有）
	const Standard_Real* X = nullptr;
	const Standard_Real* Y = nullptr;
	const Standard_Real* Z = nullptr;

	// 索引重映射。如果为 nullptr，说明是 0..Count-1 顺序访问
	const int* Indices = nullptr;

	//	视图中的元素数量
	//	dense时 = 全局点数，有Indices时 = 视图中索引数组长度
	std::size_t  Count = 0;

	Column3f()
		: Semantic(AttrSemantic::Position)
		, X(nullptr), Y(nullptr), Z(nullptr)
		, Indices(nullptr)
		, Count(0)
	{
	}

	bool IsValid() const
	{
		return X != nullptr && Y != nullptr && Z != nullptr && Count > 0;
	}

	bool IsDense() const
	{
		return Indices == nullptr;
	}

	char* AttrSemanticToString(AttrSemantic semantic) const
	{
		switch (semantic) {
		case AttrSemantic::Position:
			return "Position";
		case AttrSemantic::Normal:
			return "Normal";
		case AttrSemantic::Color:
			return "Color";
		case AttrSemantic::Intensity:
			return "Intensity";
		case AttrSemantic::Classification:
			return "Classification";
		default:
			return "Unknown";
		}
	}

	void print10() const {
		printf("Column3f: %s, Count: %d\n", AttrSemanticToString(Semantic), Count);

		const int max_print = (int)Count < 10 ? Count : 10;
		for (int i = 0; i < max_print; ++i) {
			printf("\t(%f, %f, %f)\n", X[i], Y[i], Z[i]);
		}
	}
};