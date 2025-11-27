#include "LeafProjector.hxx"

static inline void boxCorners(const Bnd_Box& b, gp_Pnt C[8])
{
	Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
	b.Get(xmin, ymin, zmin, xmax, ymax, zmax);
	C[0] = gp_Pnt(xmin, ymin, zmin); C[1] = gp_Pnt(xmax, ymin, zmin);
	C[2] = gp_Pnt(xmin, ymax, zmin); C[3] = gp_Pnt(xmax, ymax, zmin);
	C[4] = gp_Pnt(xmin, ymin, zmax); C[5] = gp_Pnt(xmax, ymin, zmax);
	C[6] = gp_Pnt(xmin, ymax, zmax); C[7] = gp_Pnt(xmax, ymax, zmax);
}

Standard_Real LeafProjector::PixelArea(const Bnd_Box& box,
	const opencascade::handle<V3d_View>& view)
{
	if (box.IsVoid() || view.IsNull()) return 0.0;

	gp_Pnt C[8];

	boxCorners(box, C);
	Standard_Integer w = 1, h = 1;
	if (!view->Window().IsNull()) { view->Window()->Size(w, h); }

	Standard_Real xmin = 1e100, ymin = 1e100, xmax = -1e100, ymax = -1e100;
	for (int i = 0; i < 8; ++i)
	{
		Standard_Real Xi = 0.0, Yi = 0.0;
		view->Project(C[i].X(), C[i].Y(), C[i].Z(), Xi, Yi); // ← 替换 Convert
		xmin = std::min(xmin, Xi); ymin = std::min(ymin, Yi);
		xmax = std::max(xmax, Xi); ymax = std::max(ymax, Yi);
	}

	// 夹紧到窗口内
	xmin = std::max<Standard_Real>(0.0, std::min<Standard_Real>(xmin, w));
	xmax = std::max<Standard_Real>(0.0, std::min<Standard_Real>(xmax, w));
	ymin = std::max<Standard_Real>(0.0, std::min<Standard_Real>(ymin, h));
	ymax = std::max<Standard_Real>(0.0, std::min<Standard_Real>(ymax, h));

	const Standard_Real area = std::max<Standard_Real>(0.0, (xmax - xmin))
		* std::max<Standard_Real>(0.0, (ymax - ymin));
	return area;
}

double LeafProjector::PixelDiag(const Handle(V3d_View)& view,
	const Bnd_Box& box,
	int haloPx)
{
	if (box.IsVoid()) return 0.0;

	Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
	box.Get(xmin, ymin, zmin, xmax, ymax, zmax);

	const gp_Pnt corners[8] = {
	  {xmin,ymin,zmin},{xmax,ymin,zmin},{xmin,ymax,zmin},{xmax,ymax,zmin},
	  {xmin,ymin,zmax},{xmax,ymin,zmax},{xmin,ymax,zmax},{xmax,ymax,zmax}
	};

	Standard_Integer ix = 0, iy = 0;
	int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;

	for (const gp_Pnt& p : corners)
	{
		// OCCT 7.8：三维转屏幕像素
		view->Convert(p.X(), p.Y(), p.Z(), ix, iy);
		minx = std::min(minx, (int)ix);
		miny = std::min(miny, (int)iy);
		maxx = std::max(maxx, (int)ix);
		maxy = std::max(maxy, (int)iy);
	}
	const int dx = (maxx - minx) + 2 * haloPx;
	const int dy = (maxy - miny) + 2 * haloPx;
	return std::sqrt(double(dx * dx + dy * dy));
}