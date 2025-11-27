// ColumnTile.hxx
#pragma once
#include "Column.hxx"
#include <vector>
#include <Bnd_Box.hxx>
#include <Standard_Real.hxx>
#include <Graphic3d_ArrayOfPoints.hxx>

struct TileLODLevel
{
	int Level = 0;          // 0 = full res

	Column3f Position;
	Column3f Normal;

	// 当前 LOD 的采样索引（索引的是「全局 SoA 数组」）
	std::vector<int> Indices;

	std::size_t PointCount = 0;

	// 世界空间误差（单位 = 模型单位，比如 mm）
	// 理解为：该 LOD 能达到的典型几何精度（点间距 / 近似误差）
	Standard_Real ErrorWorld = 0.0f;

	TileLODLevel()
		: Level(0)
		, Position()
		, Normal()
		, PointCount(0)
		, ErrorWorld(0.0f)
	{
	}

	bool IsValid() const
	{
		return Position.IsValid() && PointCount > 0 && !Indices.empty();
	}

	void print() const
	{
		Position.print10();
		Normal.print10();
		printf("Level %d, PointCount %d\n", Level, PointCount);
	}
};

struct ColumnTile
{
	//	int     TileId = -1;
	int     Depth = 0;
	Bnd_Box BBox;

	// 该 tile 在「全局 SoA 数组」中的点索引（全分辨率）
	std::vector<int> Indices;

	// 所有 LOD 级别
	std::vector<TileLODLevel> LODs;

	// 与 LODs 同长度，每个元素对应某一级 LOD 的 GPU 数组
	std::vector<Handle(Graphic3d_ArrayOfPoints)> LodArrays;

	// 当前使用哪一个 LOD（索引到 LODs / LodArrays），-1 表示还未选择
	int CurrentLOD = -1;

	// 该 tile 是否参与绘制
	bool Visible = true;

	const TileLODLevel* Level(int level) const
	{
		for (const auto& l : LODs)
			if (l.Level == level) return &l;
		return nullptr;
	}

	TileLODLevel* Level(int level)
	{
		for (auto& l : LODs)
			if (l.Level == level) return &l;
		return nullptr;
	}

	void print() const
	{
		printf("Depth %d, PointCount %d\n", /*TileId, */Depth, (int)Indices.size());
		for (const auto& l : LODs)
			l.print();
	}
};