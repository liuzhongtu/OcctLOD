// CloudColumns.hxx
#pragma once
#include "Column.hxx"
#include "CloudDataStore.hxx"
#include <Standard_Real.hxx>

struct CloudColumns
{
	Column3f Position;
	Column3f Normal; // Normal.Count==0 表示没有法向
	bool     HasNormal = false;
};

// 从 CloudDataStore 构建列视图
inline CloudColumns BuildCloudColumns(const CloudDataStore& store)
{
	CloudColumns cols;

	CloudSoAView soa = store.SoA();
	const std::size_t n = soa.Size;
	if (n == 0)
		return cols;

	// POS 列
	cols.Position.Semantic = AttrSemantic::Position;
	cols.Position.X = reinterpret_cast<const Standard_Real*>(soa.X);
	cols.Position.Y = reinterpret_cast<const Standard_Real*>(soa.Y);
	cols.Position.Z = reinterpret_cast<const Standard_Real*>(soa.Z);
	cols.Position.Indices = nullptr; // 0..n-1
	cols.Position.Count = n;

	// NORM 列（如果有）
	if (soa.HasNormals())
	{
		cols.Normal.Semantic = AttrSemantic::Normal;
		cols.Normal.X = reinterpret_cast<const Standard_Real*>(soa.NX);
		cols.Normal.Y = reinterpret_cast<const Standard_Real*>(soa.NY);
		cols.Normal.Z = reinterpret_cast<const Standard_Real*>(soa.NZ);
		cols.Normal.Indices = nullptr;
		cols.Normal.Count = n;
		cols.HasNormal = true;
	}
	else
	{
		cols.Normal.Semantic = AttrSemantic::Normal;
		cols.Normal.X = cols.Normal.Y = cols.Normal.Z = nullptr;
		cols.Normal.Indices = nullptr;
		cols.Normal.Count = 0;
		cols.HasNormal = false;
	}

	return cols;
}