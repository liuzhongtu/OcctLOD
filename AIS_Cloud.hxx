// AIS_Cloud.hxx

#pragma once

#include <AIS_InteractiveObject.hxx>
#include <Prs3d_Presentation.hxx>
#include <Prs3d_Root.hxx>
#include <Graphic3d_ArrayOfPoints.hxx>
#include <Standard_Type.hxx>
#include <Standard_Handle.hxx>

#include "CloudDataStore.hxx"
#include "CloudColumns.hxx"
#include "ColumnTile.hxx"
#include "CloudTilingColumns.hxx"

DEFINE_STANDARD_HANDLE(AIS_Cloud, AIS_InteractiveObject)

class AIS_Cloud : public AIS_InteractiveObject
{
	DEFINE_STANDARD_RTTIEXT(AIS_Cloud, AIS_InteractiveObject)

public:
	AIS_Cloud();

	// 设置点云数据
	void SetDataStore(const std::shared_ptr<CloudDataStore>& store);

	// 让外部把当前 View 注入进来（方便拿 Camera 和窗口大小）
	void SetView(const Handle(V3d_View)& theView)
	{
		myView = theView;
	}

	std::size_t NbPoints() const { return m_store ? m_store->Size() : 0; }

	int LastNumDisplayedTiles()  const { return myLastNumDisplayedTiles; }
	int LastNumDisplayedPoints() const { return myLastNumDisplayedPoints; }

	// CloudLodController 用：保证某个 tile 的某一级 LOD 已经有 GArray 缓存
	Handle(Graphic3d_ArrayOfPoints)
		EnsureTileLODArray(ColumnTile& tile, int lodIndex);

	const std::vector<ColumnTile>& Tiles() const { return myTiles; }
	std::vector<ColumnTile>& Tiles() { return myTiles; }

protected:
	void Compute(const Handle(PrsMgr_PresentationManager)& thePM,
		const Handle(Prs3d_Presentation)& thePrs,
		const Standard_Integer theMode) override;

	void ComputeSelection(
		const Handle(SelectMgr_Selection)& /*theSel*/,
		const Standard_Integer             /*theMode*/) {
	}

	void ensureTileGArray_(const ColumnTile& tile,
		const TileLODLevel& lod,
		Handle(Graphic3d_ArrayOfPoints)& outArr) const;

	void setAspect(Handle(Graphic3d_Group) theGroup);

private:
	std::shared_ptr<CloudDataStore>  m_store;
	CloudColumns            myColumns;
	std::vector<ColumnTile> myTiles;
	TilingParams            myTilingParams;

	int                     myMaxLODLevel = 2;

	int myLastNumDisplayedTiles = 0;
	int myLastNumDisplayedPoints = 0;

	Handle(V3d_View)        myView;
};