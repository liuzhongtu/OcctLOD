// ColumnTileLOD.hxx
#pragma once
#include "ColumnTile.hxx"
#include <cmath>
#include <algorithm>

// tile 的包围盒对角线长度（世界坐标）
inline double TileDiagonal(const ColumnTile& tile)
{
	Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
	tile.BBox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
	const double dx = xmax - xmin;
	const double dy = ymax - ymin;
	const double dz = zmax - zmin;
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// 为每个 ColumnTile 生成多级 LOD
// tiles          : 输入 tiles（每个 tile 有 Indices + BBox）
// maxLevel       : 最大 LOD 层数（比如 AIS_Cloud 里 myMaxLODLevel）
// minPointsPerLOD: 每级 LOD 至少多少点（避免太稀）
// 注意：本函数假定 columns.Position / Normal 已经绑定了全局 SoA 数据。
// 在给定 tiles 上基于 CloudColumns 构建 LOD 级别
inline void BuildLODsForTiles(
	const CloudColumns& columns,
	std::vector<ColumnTile>& tiles,
	int maxLODLevel,
	float baseWorldError)   // 目前暂未严格用 error，只做占位
{
	const Column3f& pos = columns.Position;
	const Column3f& nrm = columns.Normal;

	if (!pos.IsValid())
		return;

	const bool hasNormal = columns.HasNormal && nrm.IsValid();

	for (auto& tile : tiles)
	{
		if (tile.Indices.empty())
			continue;

		tile.LODs.clear();

		// -------- LOD0: full resolution --------
		{
			TileLODLevel lvl0;
			lvl0.Level = 0;

			// Position 指向全局 SoA
			lvl0.Position.Semantic = AttrSemantic::Position;
			lvl0.Position.X = pos.X;
			lvl0.Position.Y = pos.Y;
			lvl0.Position.Z = pos.Z;
			// 用 tile 的索引子集
			lvl0.Position.Indices = tile.Indices.data();
			lvl0.Position.Count = tile.Indices.size();

			if (hasNormal)
			{
				lvl0.Normal.Semantic = AttrSemantic::Normal;
				lvl0.Normal.X = nrm.X;
				lvl0.Normal.Y = nrm.Y;
				lvl0.Normal.Z = nrm.Z;
				lvl0.Normal.Indices = tile.Indices.data();
				lvl0.Normal.Count = tile.Indices.size();
			}
			else
			{
				lvl0.Normal.Semantic = AttrSemantic::Normal;
				lvl0.Normal.X = lvl0.Normal.Y = lvl0.Normal.Z = nullptr;
				lvl0.Normal.Indices = nullptr;
				lvl0.Normal.Count = 0;
			}

			lvl0.Indices = tile.Indices;   // 保存一份索引（便于调试/扩展）
			lvl0.PointCount = lvl0.Indices.size();

			tile.LODs.push_back(std::move(lvl0));
		}

		// -------- 更粗的 LOD: 均匀 stride 采样 --------
		const int maxLevel = std::max(0, maxLODLevel);
		const std::size_t fullCount = tile.Indices.size();

		for (int level = 1; level <= maxLevel; ++level)
		{
			// 简单策略：stride = 2^level
			const std::size_t stride = (std::size_t)1 << level;
			if (stride >= fullCount)
				break;  // 再下去就没必要了

			TileLODLevel lvl;
			lvl.Level = level;

			// 构造该 LOD 的采样索引
			lvl.Indices.reserve((fullCount + stride - 1) / stride);
			for (std::size_t i = 0; i < fullCount; i += stride)
			{
				lvl.Indices.push_back(tile.Indices[i]);
			}

			lvl.PointCount = lvl.Indices.size();
			if (lvl.PointCount == 0)
				continue;

			// Position 指向全局 SoA，Indices 用本 LOD 的索引数组
			lvl.Position.Semantic = AttrSemantic::Position;
			lvl.Position.X = pos.X;
			lvl.Position.Y = pos.Y;
			lvl.Position.Z = pos.Z;
			lvl.Position.Indices = lvl.Indices.data();
			lvl.Position.Count = lvl.PointCount;

			if (hasNormal)
			{
				lvl.Normal.Semantic = AttrSemantic::Normal;
				lvl.Normal.X = nrm.X;
				lvl.Normal.Y = nrm.Y;
				lvl.Normal.Z = nrm.Z;
				lvl.Normal.Indices = lvl.Indices.data();
				lvl.Normal.Count = lvl.PointCount;
			}
			else
			{
				lvl.Normal.Semantic = AttrSemantic::Normal;
				lvl.Normal.X = lvl.Normal.Y = lvl.Normal.Z = nullptr;
				lvl.Normal.Indices = nullptr;
				lvl.Normal.Count = 0;
			}

			tile.LODs.push_back(std::move(lvl));
		}

		// debug
		// tile.print();
	}
}