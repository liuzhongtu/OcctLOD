// CloudTilingColumns.cxx
#include "CloudTilingColumns.hxx"
#include <algorithm>

namespace {
	// 从 Position 列和索引集合计算 BBox
	static Bnd_Box ComputeBBoxColumn(
		const Column3f& pos,
		const std::vector<int>& idx)
	{
		Bnd_Box box;
		box.SetVoid();

		if (!pos.IsValid()) return box;
		const bool dense = pos.IsDense();
		const std::size_t nGlobal = pos.Count;

		if (dense && idx.empty())
			return box;

		for (int id : idx)
		{
			int gi = id;
			if (!dense)
			{
				// 稀疏：Indices 指向全局索引
				if (!pos.Indices) continue;
				if (id < 0 || (std::size_t)id >= pos.Count) continue;
				gi = pos.Indices[id];
			}

			if (gi < 0 || (std::size_t)gi >= nGlobal) continue;

			box.Add(gp_Pnt(pos.X[gi], pos.Y[gi], pos.Z[gi]));
		}

		return box;
	}

	// octree 八叉划分：只看 Position 列
	static void Partition8Column(
		const Column3f& pos,
		const std::vector<int>& inIdx,
		const Bnd_Box& box,
		std::vector<int> outIdx[8])
	{
		Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
		box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
		const Standard_Real cx = Standard_Real(0.5 * (xmin + xmax));
		const Standard_Real cy = Standard_Real(0.5 * (ymin + ymax));
		const Standard_Real cz = Standard_Real(0.5 * (zmin + zmax));

		const std::size_t nGlobal = pos.Count;

		for (int id : inIdx)
		{
			if (id < 0 || (std::size_t)id >= nGlobal) continue;
			const Standard_Real x = pos.X[id];
			const Standard_Real y = pos.Y[id];
			const Standard_Real z = pos.Z[id];

			int oct = (x >= cx ? 1 : 0)
				| (y >= cy ? 2 : 0)
				| (z >= cz ? 4 : 0);
			outIdx[oct].push_back(id);
		}
	}

	// KD 分割：按某一轴的坐标做中位数
	static void PartitionMedianAxisColumn(
		const Column3f& pos,
		std::vector<int>& inOutIdx,
		int axis,
		std::vector<int>& leftIdx,
		std::vector<int>& rightIdx)
	{
		auto coord = [&](int id)->Standard_Real {
			switch (axis)
			{
			case 0: return pos.X[id];
			case 1: return pos.Y[id];
			default:return pos.Z[id];
			}
			};

		const std::size_t mid = inOutIdx.size() / 2;
		std::nth_element(
			inOutIdx.begin(),
			inOutIdx.begin() + mid,
			inOutIdx.end(),
			[&](int a, int b) { return coord(a) < coord(b); });

		leftIdx.assign(inOutIdx.begin(), inOutIdx.begin() + mid);
		rightIdx.assign(inOutIdx.begin() + mid, inOutIdx.end());
	}

	// 选择最长轴
	static int LongestAxis(const Bnd_Box& box)
	{
		Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
		box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
		const double dx = xmax - xmin;
		const double dy = ymax - ymin;
		const double dz = zmax - zmin;
		if (dx >= dy && dx >= dz) return 0;
		if (dy >= dx && dy >= dz) return 1;
		return 2;
	}

	// 分裂停止条件
	static bool StopSplit(int numPoints, int depth, const TilingParams& params)
	{
		if (numPoints <= params.LeafMaxPoints) return true;
		if (depth >= params.MaxDepth) return true;
		return false;
	}
} // namespace

void CloudTilingColumns::buildOctreeRecursive(
	const CloudColumns& columns,
	const std::vector<int>& inIdx,
	const Bnd_Box& inBox,
	int depth,
	const TilingParams& params,
	std::vector<ColumnTile>& outTiles)
{
	const Column3f& pos = columns.Position;
	if (!pos.IsValid() || inIdx.empty()) return;

	if (StopSplit((int)inIdx.size(), depth, params))
	{
		// 叶子节点 -> ColumnTile
		ColumnTile tile;
		tile.Depth = depth;
		tile.Indices = inIdx;
		tile.BBox = inBox.IsVoid() ? ComputeBBoxColumn(pos, inIdx) : inBox;

		// 构建 LOD Level 0
		TileLODLevel lvl0;
		lvl0.Level = 0;
		lvl0.PointCount = tile.Indices.size();

		// Position LOD0：指向全局 SoA + 本 Tile 的索引
		lvl0.Position.Semantic = AttrSemantic::Position;
		lvl0.Position.X = pos.X;
		lvl0.Position.Y = pos.Y;
		lvl0.Position.Z = pos.Z;
		lvl0.Position.Indices = nullptr;
		lvl0.Position.Count = pos.Count;   // 全局点数

		// Normal LOD0（如果有）
		if (columns.HasNormal && columns.Normal.IsValid())
		{
			lvl0.Normal.Semantic = AttrSemantic::Normal;
			lvl0.Normal.X = columns.Normal.X;
			lvl0.Normal.Y = columns.Normal.Y;
			lvl0.Normal.Z = columns.Normal.Z;
			lvl0.Normal.Indices = nullptr;
			lvl0.Normal.Count = columns.Normal.Count;
		}
		else
		{
			lvl0.Normal.Semantic = AttrSemantic::Normal;
			lvl0.Normal.X = lvl0.Normal.Y = lvl0.Normal.Z = nullptr;
			lvl0.Normal.Indices = nullptr;
			lvl0.Normal.Count = 0;
		}

		//		lvl0.print();

		tile.LODs.push_back(lvl0);
		outTiles.push_back(std::move(tile));
		return;
	}

	// 非叶子节点：继续分裂
	Bnd_Box box = inBox.IsVoid() ? ComputeBBoxColumn(pos, inIdx) : inBox;

	std::vector<int> childIdx[8];
	for (int i = 0; i < 8; ++i)
		childIdx[i].reserve(inIdx.size() / 4 + 1);

	Partition8Column(pos, inIdx, box, childIdx);

	for (int i = 0; i < 8; ++i)
	{
		if (childIdx[i].empty()) continue;
		Bnd_Box cbox = ComputeBBoxColumn(pos, childIdx[i]);
		buildOctreeRecursive(columns, childIdx[i], cbox,
			depth + 1, params, outTiles);
	}
}

void CloudTilingColumns::BuildOctree(
	const CloudColumns& columns,
	std::vector<ColumnTile>& outTiles,
	TilingStatsColumns& stats,
	const TilingParams& params)
{
	outTiles.clear();
	stats = {};

	if (!columns.Position.IsValid())
		return;

	const std::size_t n = columns.Position.Count;
	std::vector<int> rootIdx(n);
	for (std::size_t i = 0; i < n; ++i)
		rootIdx[i] = (int)i;

	Bnd_Box rootBox = ComputeBBoxColumn(columns.Position, rootIdx);

	buildOctreeRecursive(columns, rootIdx, rootBox, 0, params, outTiles);

	stats.NumTiles = (int)outTiles.size();
	stats.TotalPoints = n;
}