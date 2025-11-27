// CloudTilingColumns.hxx
#pragma once
#include "CloudColumns.hxx"
#include "ColumnTile.hxx"

struct TilingParams
{
	int LeafMaxPoints = 4096;
	int MaxDepth = 12;
};

struct TilingStatsColumns
{
	int         NumTiles = 0;
	std::size_t TotalPoints = 0;
};

// 基于 CloudColumns 的 AoS+SoA 视图，构建 ColumnTile 八叉树/KD树 叶子
class CloudTilingColumns
{
public:
	// 直接用 CloudColumns（而不是 CloudDataStore）
	static void BuildOctree(
		const CloudColumns& columns,
		std::vector<ColumnTile>& outTiles,
		TilingStatsColumns& stats,
		const TilingParams& params);

	static void BuildKDTree(
		const CloudColumns& columns,
		std::vector<ColumnTile>& outTiles,
		TilingStatsColumns& stats,
		const TilingParams& params);

private:
	static void buildOctreeRecursive(
		const CloudColumns& columns,
		const std::vector<int>& inIdx,
		const Bnd_Box& inBox,
		int depth,
		const TilingParams& params,
		std::vector<ColumnTile>& outTiles);

	static void buildKDRecursive(
		const CloudColumns& columns,
		std::vector<int>& inOutIdx,
		const Bnd_Box& inBox,
		int depth,
		const TilingParams& params,
		std::vector<ColumnTile>& outTiles);
};