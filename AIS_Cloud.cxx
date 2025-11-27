#include "AIS_Cloud.hxx"
#include <Graphic3d_AspectMarker3d.hxx>
#include <Prs3d_PointAspect.hxx>
#include <Prs3d_Presentation.hxx>
#include <Graphic3d_Group.hxx>
#include <SelectMgr_Selection.hxx>
#include <BRep_Builder.hxx>
#include <V3d_View.hxx>
#include "lod\ColumnTileLOD.hxx"

static const std::vector<Quantity_Color> s_colorList = {
	Quantity_Color(240 / 255.0, 200 / 255.0, 0 / 255.0, Quantity_TOC_sRGB),	// 默认颜色
	Quantity_Color(126 / 255.0,  240 / 255.0, 191 / 255.0, Quantity_TOC_sRGB),	//
	Quantity_Color(255 / 255.0, 219 / 255.0, 177 / 255.0, Quantity_TOC_sRGB),	//
	Quantity_Color(157 / 255.0, 157 / 255.0, 255 / 255.0, Quantity_TOC_sRGB),	//
	Quantity_Color(220 / 255.0, 255 / 255.0, 119 / 255.0, Quantity_TOC_sRGB),	//

	Quantity_Color(201 / 255.0, 151 / 255.0, 255 / 255.0, Quantity_TOC_sRGB),	//
	Quantity_Color(153 / 255.0, 255 / 255.0, 251 / 255.0, Quantity_TOC_sRGB),	//
	Quantity_Color(180 / 255.0, 208 / 255.0, 255 / 255.0, Quantity_TOC_sRGB),	//
};	// 颜色列表
static size_t s_colorIdx = 0;	// 颜色索引

IMPLEMENT_STANDARD_RTTIEXT(AIS_Cloud, AIS_InteractiveObject)

AIS_Cloud::AIS_Cloud()
{
	s_colorIdx++;
	this->Attributes()->SetShadingModel(Graphic3d_TOSM_FRAGMENT, true);
}

void AIS_Cloud::SetDataStore(const std::shared_ptr<CloudDataStore>& store)
{
	m_store = store;
	myTiles.clear();
	myColumns = {};

	if (m_store == nullptr) {
		SetToUpdate();
		return;
	}

	// 1) 从 CloudDataStore 的 SoA 构建 Column 视图
	myColumns = BuildCloudColumns(*m_store);

	// 2) 基于 Column 做空间划分（octree / KDtree）
	TilingStatsColumns stats;
	CloudTilingColumns::BuildOctree(
		myColumns,
		myTiles,
		stats,
		myTilingParams);       // LeafMaxPoints / MaxDepth 等参数在 myTilingParams 里

	// 3) 为每个 Tile 构建各级 LOD，并在内部计算每级的 ErrorWorld
	BuildLODsForTiles(
		myColumns,
		myTiles,
		myMaxLODLevel,         // 比如 2 或 3
		2.0f);                 // 预留出来的世界误差参数

	// 初始化 LOD 缓存和状态
	for (auto& tile : myTiles)
	{
		tile.LodArrays.clear();
		tile.LodArrays.resize(tile.LODs.size()); // 与 LODs 同步
		tile.CurrentLOD = 0;     // 默认用 LOD0
		tile.Visible = false;  // 默认都不可见
	}

	// 4) 通知 OCCT 重新生成展示
	SetToUpdate();	//	存疑，有效果吗？
}

void AIS_Cloud::ensureTileGArray_(
	const ColumnTile& tile,
	const TileLODLevel& lod,
	Handle(Graphic3d_ArrayOfPoints)& outArr) const
{
	const Column3f& pos = lod.Position;
	const Column3f& ncol = lod.Normal;

	if (!pos.IsValid()) {
		// 把关键字段都看一眼
		__debugbreak(); // 在这里停下看：pos.X / pos.Y / pos.Z / pos.Count / pos.Indices
		return;
	}

	if (outArr.IsNull())
	{
		const int N = (int)lod.PointCount;
		if (N <= 0)
			return;

		outArr = new Graphic3d_ArrayOfPoints(
			N, Standard_False, Standard_True);
	}

	const int* idx = pos.Indices;
	const bool hasIdx = (idx != nullptr);

	// 全局点数：从 CloudDataStore 拿，更可信
	const std::size_t globalCount =
		m_store ? m_store->Size() : pos.Count;

	for (std::size_t i = 0; i < lod.PointCount; ++i)
	{
		int pid = hasIdx ? idx[i] : (int)i;
		if (pid < 0 || (std::size_t)pid >= globalCount)
			continue;

		const Standard_Real x = pos.X[pid];
		const Standard_Real y = pos.Y[pid];
		const Standard_Real z = pos.Z[pid];
		gp_Pnt p(x, y, z);

		gp_Dir nrm(0.0, 0.0, 1.0);
		if (ncol.IsValid() && globalCount > (std::size_t)pid)
		{
			const Standard_Real nx = ncol.X[pid];
			const Standard_Real ny = ncol.Y[pid];
			const Standard_Real nz = ncol.Z[pid];
			nrm.SetCoord(nx, ny, nz);
		}

		outArr->AddVertex(p, nrm);
	}
}

void printPrimitive10Pts(Handle(Graphic3d_ArrayOfPoints) arr)
{
	if (arr.IsNull() || arr->VertexNumber() <= 0)
		return;

	std::cout << "[Displayed cloud points 10]: " << std::endl;
	int max_print = arr->VertexNumber() < 10 ? arr->VertexNumber() : 10;
	for (int rank = 1; rank <= max_print; ++rank)
	{
		gp_Pnt p = arr->Vertice(rank);
		std::cout << "\tPoint " << rank << ": " << p.X() << ", " << p.Y() << ", " << p.Z() << std::endl;
	}
}

void AIS_Cloud::setAspect(Handle(Graphic3d_Group) theGroup)
{
	Handle(Prs3d_PointAspect) aspect = Attributes()->PointAspect();
	Handle(Graphic3d_AspectMarker3d) aMarker = new Graphic3d_AspectMarker3d(
		Aspect_TOM_POINT,
		s_colorList[(s_colorIdx - 1) % s_colorList.size()],
		3);
	aMarker->SetShadingModel(Graphic3d_TypeOfShadingModel_DEFAULT);
	this->SetMaterial(Graphic3d_NOM_PLASTIC);

	// 设置点云样式
	theGroup->SetGroupPrimitivesAspect(aMarker);
}

Handle(Graphic3d_ArrayOfPoints)
AIS_Cloud::EnsureTileLODArray(ColumnTile& tile, int lodIndex)
{
	if (lodIndex < 0 || lodIndex >= (int)tile.LODs.size())
		return Handle(Graphic3d_ArrayOfPoints)();

	// 与 LODs 数量对齐
	if (tile.LodArrays.size() != tile.LODs.size())
		tile.LodArrays.resize(tile.LODs.size());

	Handle(Graphic3d_ArrayOfPoints)& arr = tile.LodArrays[lodIndex];
	if (!arr.IsNull())
		return arr; // 已有缓存

	const TileLODLevel& lod = tile.LODs[lodIndex];

	ensureTileGArray_(tile, lod, arr);

	return arr;
}

void AIS_Cloud::Compute(const Handle(PrsMgr_PresentationManager)& thePM,
	const Handle(Prs3d_Presentation)& thePrs,
	const Standard_Integer theMode)
{
	thePrs->Clear();

	Handle(Graphic3d_Group) aGroup = thePrs->NewGroup();
	setAspect(aGroup);

	// 打印可显示的tile数量
	int numDisplayedTiles = 0;
	int numDisplayedPoints = 0;

	int idx = 0;
	for (auto& tile : myTiles)
	{
		if (!tile.Visible)
			continue;

		int lvl = tile.CurrentLOD;
		if (lvl < 0 || lvl >= (int)tile.LODs.size())
			continue;

		Handle(Graphic3d_ArrayOfPoints) arr = EnsureTileLODArray(tile, lvl);
		if (arr.IsNull())
			continue;

		aGroup->AddPrimitiveArray(arr);

		idx++;
		numDisplayedPoints += arr->VertexNumber();

		// 可选：调试打印
		// printPrimitive10Pts(arr);
	}

	// 记录到成员变量，供 HUD 使用
	myLastNumDisplayedTiles = idx;
	myLastNumDisplayedPoints = numDisplayedPoints;
}