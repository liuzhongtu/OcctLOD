#include "CloudLodController.hxx"
#include "LeafProjector.hxx"

#include "AIS_Cloud.hxx"
#include <Standard_Type.hxx>
#include <Bnd_Box.hxx>
#include <NCollection_List.hxx>
#include <algorithm>
#include <unordered_set>
#include <cmath>

// ----------- AIS_Cloud 需要暴露的最小接口 ------------
// 1) 获取根节点（没有就提供“顶层节点”），返回 TileLeaf* 列表
// 2) 针对某节点：确保某个 stride 的 Rep 已构建（懒创建）
// 3) 显示/隐藏 “节点+repIdx”
// 4) 查询节点是否叶子、子节点列表、节点已有的 Reps（stride/pointCount/garr）
//

// 1) 返回节点的包围盒
static const Bnd_Box& TL_Box(const ColumnTile& n)
{
	return n.BBox;
}

// 2) 是否叶子：目前是平铺 tiles，就直接返回 true；
// 如果以后支持树结构，再改成 !n.Children.empty()
static bool TL_IsLeaf(const ColumnTile& n)
{
	return n.Children.empty();
}

static std::vector<ColumnTile*> Cloud_GetRoots(const Handle(AIS_Cloud)& cloud)
{
	std::vector<ColumnTile*> roots;
	if (cloud.IsNull())
		return roots;

	auto& tiles = cloud->Tiles();
	roots.reserve(tiles.size());
	for (std::size_t i = 0; i < tiles.size(); ++i)
	{
		auto& t = tiles[i];
		if (t.Parent < 0)
			roots.push_back(&t);
	}
	return roots;
}

static void Cloud_BuildRepIfMissing(const Handle(AIS_Cloud)& cloud,
	ColumnTile& node,
	int repIdx)
{
	if (cloud.IsNull())
		return;

	cloud->EnsureTileLODArray(node, repIdx);
}

static void Cloud_ShowNodeRep(const Handle(AIS_Cloud)& cloud,
	ColumnTile& node,
	int repIdx)
{
	if (cloud.IsNull())
		return;

	// 确保该 LOD 有 GArray
	cloud->EnsureTileLODArray(node, repIdx);

	node.Visible = true;
	node.CurrentLOD = repIdx;

	// 标记 AIS_Cloud 需要重算
	cloud->SetToUpdate();
}

static void Cloud_HideNodeRep(const Handle(AIS_Cloud)& cloud,
	ColumnTile& node,
	int /*repIdx*/)
{
	if (cloud.IsNull())
		return;

	node.Visible = false;
	node.CurrentLOD = -1;

	cloud->SetToUpdate();
}

// ----------------- Controller 实现 -----------------

CloudLodController::CloudLodController(const Handle(AIS_InteractiveContext)& ctx,
	const Handle(V3d_View)& view)
	: m_ctx(ctx), m_view(view)
{
}

CloudLodController::~CloudLodController() = default;

void CloudLodController::RegisterCloud(const Handle(AIS_Cloud)& cloud)
{
	CloudEntry e;
	e.cloud = cloud;
	e.roots = Cloud_GetRoots(cloud); // TODO
	m_clouds.push_back(std::move(e));
}

void CloudLodController::UnregisterCloud(const Handle(AIS_Cloud)& cloud)
{
	m_clouds.erase(std::remove_if(m_clouds.begin(), m_clouds.end(),
		[&](const CloudEntry& ce) { return ce.cloud == cloud; }), m_clouds.end());
}

static int chooseRepIdx_(const ColumnTile& node,
	double pixDiag,
	const CloudLodController::LodThreshold& th,
	int lastRepIdx)
{
	std::vector<RepLevel> reps = TL_Reps(node);
	if (reps.empty())
		return -1;

	const int maxIdx = (int)reps.size() - 1;

	// 1) 先按“无 hysteresis”方式算一个目标 LOD
	int baseIdx = 0;
	if (pixDiag <= th.pixDiagCoarse)
		baseIdx = maxIdx; // 最粗
	else if (pixDiag >= th.pixDiagFine)
		baseIdx = 0;      // 最细
	else
	{
		double t = (pixDiag - th.pixDiagCoarse) / (th.pixDiagFine - th.pixDiagCoarse);
		baseIdx = (int)std::round((1.0 - t) * maxIdx);
	}

	if (baseIdx < 0)      baseIdx = 0;
	if (baseIdx > maxIdx) baseIdx = maxIdx;

	// 2) 如果还没有历史 LOD，就直接用 baseIdx
	if (lastRepIdx < 0 || lastRepIdx > maxIdx)
		return baseIdx;

	// 3) 引入 hysteresis：
	//    - 变“更细”：需要 pixDiag 明显变大一点
	//    - 变“更粗”：需要 pixDiag 明显变小一点
	double h = th.hysteresis <= 0.0 ? 1.0 : th.hysteresis;

	int resultIdx = lastRepIdx;

	if (baseIdx < lastRepIdx)
	{
		// 想变细（索引减小），要求像素变大到 Fine * h 以上
		if (pixDiag >= th.pixDiagFine * h)
			resultIdx = baseIdx;
	}
	else if (baseIdx > lastRepIdx)
	{
		// 想变粗（索引增大），要求像素变小到 Coarse / h 以下
		if (pixDiag <= th.pixDiagCoarse / h)
			resultIdx = baseIdx;
	}
	// 如果 baseIdx == lastRepIdx，就保持不变

	return resultIdx;
}

bool CloudLodController::Tick()
{
	auto t0 = clk::now();

	selectLOD_();
	bool anyChanged = applyDiff_();

	m_rt.selectMs = std::chrono::duration<double, std::milli>(
		clk::now() - t0).count();

	// Tick 只做逻辑，不负责 UpdateCurrentViewer
	return anyChanged;
}

void CloudLodController::selectLOD_()
{
	if (!m_useExperimental) {
		// 基线：完全等价于你现在已经验证过的行为
		selectLOD_baseline_();
	}
	else {
		// 实验路径：目前先直接调用基线，保证行为一致
		selectLOD_experimental_();
	}
}

void CloudLodController::selectLOD_experimental_()
{
	// 现在先直接复用基线逻辑，保证“打开实验开关”
	// 也不会改变结果。后面你要做新算法，就在这里改。
	selectLOD_baseline_();
}

void CloudLodController::selectLOD_baseline_()
{
	m_activeNow.clear();
	m_rt.pointsChosen = 0;
	m_rt.nodesShown = 0;

	if (m_clouds.empty() || m_view.IsNull())
		return;

	// 为每个 tile 记录一份状态，方便后面用预算统一调节 LOD
	struct TileState
	{
		Handle(AIS_Cloud) cloud;
		ColumnTile* node = nullptr;
		double                  pixDiag = 0.0;
		std::vector<int>        lodCost;   // 每个 LOD 的点数
		int                     maxIdx = 0;
		int                     desiredIdx = 0; // 按像素计算的理想 LOD
		int                     currentIdx = 0; // 经过预算调整后的实际 LOD
	};

	std::vector<TileState> tiles;
	tiles.reserve(1024);

	// -------------------------
	// 0) 统计全局点数，决定是否需要启用 LOD
	//    如果整体点数都小于预算，就没必要动 LOD，直接全用最细
	// -------------------------
	std::size_t globalPoints = 0;
	for (const auto& ce : m_clouds)
	{
		if (!ce.cloud.IsNull())
			globalPoints += (std::size_t)ce.cloud->NbPoints();
	}

	const std::int64_t budget = (std::int64_t)m_budget.maxPoints;
	const bool disableLOD = (budget <= 0) || (globalPoints <= (std::size_t)budget);

	// -------------------------
	// 1) 收集所有需要显示的 tile，计算每个 tile 的 pixDiag 和各级 LOD 的点数
	// -------------------------
	for (auto& ce : m_clouds)
	{
		if (ce.cloud.IsNull())
			continue;

		auto& allTiles = ce.cloud->Tiles();

		for (ColumnTile* root : ce.roots)
		{
			if (!root)
				continue;

			std::vector<ColumnTile*> stack;
			stack.push_back(root);

			while (!stack.empty())
			{
				ColumnTile* node = stack.back();
				stack.pop_back();
				if (!node)
					continue;

				//	太小的 tile 直接丢掉（pixDiagHide）
				const Bnd_Box& box = TL_Box(*node);
				const double pd = LeafProjector::PixelDiag(m_view, box, 0);
				if (pd <= m_th.pixDiagHide)
					continue;

				if (!TL_IsLeaf(*node))
				{
					for (int childIdx : node->Children)
					{
						if (childIdx < 0 || childIdx >= allTiles.size())
							continue;
						stack.push_back(&allTiles[childIdx]);
					}
					continue;
				}

				std::vector<RepLevel> reps = TL_Reps(*node);
				if (reps.empty())
					continue;

				TileState st;
				st.cloud = ce.cloud;
				st.node = node;
				st.pixDiag = pd;
				st.maxIdx = (int)reps.size() - 1;
				st.lodCost.resize(reps.size());
				for (std::size_t i = 0; i < reps.size(); ++i)
				{
					st.lodCost[i] = reps[i].pointCount;
				}

				// 1.2 取上一帧 LOD 作为 hysteresis 的参考
				int lastIdx = node->CurrentLOD;
				if (lastIdx < 0 || lastIdx > st.maxIdx)
					lastIdx = -1;

				int repIdx = 0;

				if (disableLOD || reps.size() == 1)
				{
					// 小点云或只有一个 LOD：一律用最细（0）
					repIdx = 0;
				}
				else
				{
					repIdx = chooseRepIdx_(*node, pd, m_th, lastIdx);
					if (repIdx < 0)            repIdx = 0;
					if (repIdx > st.maxIdx)    repIdx = st.maxIdx;
				}

				st.desiredIdx = repIdx;
				st.currentIdx = repIdx;

				tiles.push_back(std::move(st));
			}
		}
	}

	if (tiles.empty())
		return;

	// -------------------------
	// 2) 计算“按像素理想 LOD”下的总点数
	// -------------------------
	std::int64_t totalCost = 0;
	for (const TileState& st : tiles)
	{
		totalCost += (std::int64_t)st.lodCost[st.currentIdx];
	}

	// -------------------------
	// 3) 如果需要 LOD，并且总点数超出预算，
	//    用预算来驱动“变粗”操作，但不丢掉任何 tile
	// -------------------------
	if (!disableLOD && budget > 0 && totalCost > budget)
	{
		// 3.1 先算出所有 tile 全部使用最粗 LOD 时的最小可能点数
		std::int64_t minCost = 0;
		for (const TileState& st : tiles)
		{
			minCost += (std::int64_t)st.lodCost[st.maxIdx];
		}

		if (minCost >= budget)
		{
			// 即使用到最粗 LOD，总点数也压不进预算，只能接受 >budget 的结果
			// 此时我们统一降到最粗 LOD，保证不马赛克
			for (auto& st : tiles)
				st.currentIdx = st.maxIdx;
			totalCost = minCost;
		}
		else
		{
			// 3.2 可以通过调粗 LOD 把点数压进预算
			// 策略：优先对屏幕上“看起来比较小”的 tile 调粗
			std::vector<int> order(tiles.size());
			for (std::size_t i = 0; i < tiles.size(); ++i)
				order[i] = (int)i;

			std::sort(order.begin(), order.end(),
				[&](int a, int b)
				{
					return tiles[a].pixDiag < tiles[b].pixDiag; // 小的先被牺牲
				});

			bool changed = true;
			while (changed && totalCost > budget)
			{
				changed = false;

				for (int idx : order)
				{
					TileState& st = tiles[idx];
					if (st.currentIdx >= st.maxIdx)
						continue; // 已经是最粗

					const int oldIdx = st.currentIdx;
					const int newIdx = oldIdx + 1; // 向更粗迈一步

					const std::int64_t oldCost = st.lodCost[oldIdx];
					const std::int64_t newCost = st.lodCost[newIdx];
					const std::int64_t delta = oldCost - newCost;

					st.currentIdx = newIdx;
					totalCost -= delta;
					changed = true;

					if (totalCost <= budget)
						break;
				}
			}
		}
	}

	// -------------------------
	// 4) 把最终 LOD 结果写入 m_activeNow
	// -------------------------
	for (const TileState& st : tiles)
	{
		m_activeNow.push_back(NodeRep{ st.cloud, st.node, st.currentIdx });
		m_rt.pointsChosen += st.lodCost[st.currentIdx];
		++m_rt.nodesShown;
	}
}

bool CloudLodController::applyDiff_()
{
	bool anyChanged = false;

	// 记录本帧有 tile 显示/隐藏变化的 cloud
	std::vector< Handle(AIS_Cloud) > dirtyClouds;
	dirtyClouds.reserve(m_clouds.size());

	auto markDirty = [&](const Handle(AIS_Cloud)& cloud) {
		if (cloud.IsNull()) return;
		// 简单去重（数量不大，用线性查找就行）
		auto it = std::find(dirtyClouds.begin(), dirtyClouds.end(), cloud);
		if (it == dirtyClouds.end())
			dirtyClouds.push_back(cloud);
		};

	auto makeKey = [](const NodeRep& nr)->std::uintptr_t {
		const auto nodeKey = reinterpret_cast<std::uintptr_t>(nr.node);
		const auto repKey = static_cast<std::uintptr_t>(nr.repIdx * 0x9e3779b97f4a7c15ull);
		const auto cloudKey = reinterpret_cast<std::uintptr_t>(nr.cloud.get());
		return nodeKey ^ repKey ^ (cloudKey << 1);
		};

	std::unordered_set<std::uintptr_t> lastSet;
	lastSet.reserve(m_activeLast.size() * 2 + 1);
	for (const NodeRep& nr : m_activeLast) lastSet.insert(makeKey(nr));

	std::unordered_set<std::uintptr_t> nowSet;
	nowSet.reserve(m_activeNow.size() * 2 + 1);
	for (const NodeRep& nr : m_activeNow)  nowSet.insert(makeKey(nr));

	// 1) 隐藏 last - now
	for (const NodeRep& nr : m_activeLast) {
		if (nowSet.find(makeKey(nr)) == nowSet.end()) {
			if (!nr.cloud.IsNull()) {
				Cloud_HideNodeRep(nr.cloud, *nr.node, nr.repIdx);
				markDirty(nr.cloud);
				anyChanged = true;
			}
		}
	}

	// 2) 显示 now - last
	for (const NodeRep& nr : m_activeNow) {
		if (lastSet.find(makeKey(nr)) == lastSet.end()) {
			if (!nr.cloud.IsNull()) {
				Cloud_BuildRepIfMissing(nr.cloud, *nr.node, nr.repIdx);
				Cloud_ShowNodeRep(nr.cloud, *nr.node, nr.repIdx);
				markDirty(nr.cloud);
				anyChanged = true;
			}
		}
	}

	// 3) 只对“确实有 tile 变化”的 cloud 做 UpdatePresentations
	for (const auto& cloud : dirtyClouds) {
		m_ctx->Redisplay(cloud, Standard_False);
	}
	anyChanged = dirtyClouds.size() > 0;

	m_activeLast.swap(m_activeNow);
	return anyChanged;
}

void CloudLodController::UpdateDisplayedStats()
{
	int totalTiles = 0;
	int totalPoints = 0;

	for (const auto& ce : m_clouds) {
		if (ce.cloud.IsNull())
			continue;

		totalTiles += ce.cloud->LastNumDisplayedTiles();
		totalPoints += ce.cloud->LastNumDisplayedPoints();
	}

	m_hudStats.displayedTiles = totalTiles;
	m_hudStats.displayedPoints = totalPoints;
}
