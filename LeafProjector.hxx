#pragma once
#include <V3d_View.hxx>
#include <Bnd_Box.hxx>
#include <gp_Pnt.hxx>
#include <algorithm>
#include <cmath>
#include <Standard_Real.hxx>

struct LeafProjector
{
	//! 计算包围盒投影到屏幕后的像素对角线长度（带 halo）
	static double PixelDiag(const Handle(V3d_View)& view,
		const Bnd_Box& box,
		int haloPx = 0);

	static Standard_Real LeafProjector::PixelArea(const Bnd_Box& box,
		const opencascade::handle<V3d_View>& view);
};