#include "pch.h"
#include "CloudDataStore.hxx"
#include "MappedFile.hxx"

#include <charconv>
#include <cfloat>
#include <chrono>

using clk = std::chrono::high_resolution_clock;

// ---------- 内部：AABB ----------
// ---------- 内部 AABB ----------
void CloudDataStore::computeBBox_()
{
	BndAll_.SetVoid();
	if (P_.empty()) return;
	Standard_Real xmin = P_[0].X(), xmax = xmin;
	Standard_Real ymin = P_[0].Y(), ymax = ymin;
	Standard_Real zmin = P_[0].Z(), zmax = zmin;
	const size_t n = P_.size();
	for (size_t i = 1; i < n; ++i) {
		const gp_Pnt& p = P_[i];
		const Standard_Real x = p.X(), y = p.Y(), z = p.Z();
		if (x < xmin) xmin = x; else if (x > xmax) xmax = x;
		if (y < ymin) ymin = y; else if (y > ymax) ymax = y;
		if (z < zmin) zmin = z; else if (z > zmax) zmax = z;
	}
	BndAll_.Update(xmin, ymin, zmin);
	BndAll_.Update(xmax, ymax, zmax);
}

// ---------- SoA 维护 ----------
void CloudDataStore::invalidateSoA_()
{
	soaDirty_ = true;
	X_.clear(); Y_.clear(); Z_.clear();
	NX_.clear(); NY_.clear(); NZ_.clear();
}

void CloudDataStore::rebuildSoA_() const
{
	if (!soaDirty_) return;

	const size_t n = P_.size();
	X_.resize(n);
	Y_.resize(n);
	Z_.resize(n);
	for (size_t i = 0; i < n; ++i)
	{
		const gp_Pnt& p = P_[i];
		X_[i] = p.X();
		Y_[i] = p.Y();
		Z_[i] = p.Z();
	}

	if (!N_.empty())
	{
		const size_t m = N_.size();
		NX_.resize(m);
		NY_.resize(m);
		NZ_.resize(m);
		for (size_t i = 0; i < m; ++i)
		{
			const gp_Dir& d = N_[i];
			NX_[i] = d.X();
			NY_[i] = d.Y();
			NZ_[i] = d.Z();
		}
	}
	else
	{
		NX_.clear(); NY_.clear(); NZ_.clear();
	}

	soaDirty_ = false;
}

CloudSoAView CloudDataStore::SoA() const
{
	CloudSoAView view;
	if (P_.empty())
		return view;

	rebuildSoA_();

	view.X = X_.data();
	view.Y = Y_.data();
	view.Z = Z_.data();
	view.Size = X_.size();

	if (!NX_.empty() && NX_.size() == view.Size)
	{
		view.NX = NX_.data();
		view.NY = NY_.data();
		view.NZ = NZ_.data();
	}

	return view;
}

// ---------- 设置接口 ----------
// ---------- 设置接口 ----------
void CloudDataStore::SetXYZ(std::vector<gp_Pnt> pts)
{
	P_ = std::move(pts);
	N_.clear(); N_.shrink_to_fit();
	computeBBox_();
	invalidateSoA_();
}

void CloudDataStore::SetXYZN(std::vector<gp_Pnt> pts, std::vector<gp_Dir> nrm)
{
	if (pts.size() != nrm.size()) { nrm.clear(); nrm.shrink_to_fit(); } // 长度不匹配，丢掉法向
	P_ = std::move(pts);
	N_ = std::move(nrm);
	computeBBox_();
	invalidateSoA_();
}

void CloudDataStore::SetXYZAndBBox(std::vector<gp_Pnt> pts,
	Standard_Real xmin, Standard_Real xmax,
	Standard_Real ymin, Standard_Real ymax,
	Standard_Real zmin, Standard_Real zmax)
{
	P_ = std::move(pts);
	N_.clear(); N_.shrink_to_fit();
	BndAll_.SetVoid();
	BndAll_.Update(xmin, ymin, zmin);
	BndAll_.Update(xmax, ymax, zmax);
	invalidateSoA_();
}

void CloudDataStore::SetXYZNAndBBox(std::vector<gp_Pnt> pts, std::vector<gp_Dir> nrm,
	Standard_Real xmin, Standard_Real xmax,
	Standard_Real ymin, Standard_Real ymax,
	Standard_Real zmin, Standard_Real zmax)
{
	if (pts.size() != nrm.size()) { nrm.clear(); nrm.shrink_to_fit(); }
	P_ = std::move(pts);
	N_ = std::move(nrm);
	BndAll_.SetVoid();
	BndAll_.Update(xmin, ymin, zmin);
	BndAll_.Update(xmax, ymax, zmax);
	invalidateSoA_();
}

// ---------- 文本解析辅助 ----------
static inline bool parseFloat(const char*& p, const char* end, double& out) {
	while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
	if (p >= end) return false;
	auto res = std::from_chars(p, end, out);
	if (res.ec != std::errc()) return false;
	p = res.ptr;
	return true;
}
static inline void skipToEOL(const char*& p, const char* end) {
	while (p < end && *p != '\n') ++p;
	if (p < end && *p == '\n') ++p;
}

template<typename PathT>
static bool loadTxtMappedImpl(const PathT& path, CloudDataStore& self,
	int xCol, int yCol, int zCol,
	std::optional<int> nxCol,
	std::optional<int> nyCol,
	std::optional<int> nzCol,
	int totalColsPerLine)
{
	MappedView mv;
	if (!mapFile(path, mv)) return false;

	const char* ptr = mv.data;
	const char* end = mv.data + mv.size;

	// 统计行数（估算 reserve）
	size_t nLines = 0;
	for (const char* q = ptr; q < end; ++q) if (*q == '\n') ++nLines;
	if (nLines == 0) { mv.close(); return false; }

	const bool withN = (nxCol && nyCol && nzCol);
	std::vector<gp_Pnt> pts; pts.reserve(nLines);
	std::vector<gp_Dir> nrm; if (withN) nrm.reserve(nLines);

	bool first = true;
	double xmin = 0, xmax = 0, ymin = 0, ymax = 0, zmin = 0, zmax = 0;

	// 主循环
	while (ptr < end) {
		if (*ptr == '\n') { ++ptr; continue; }
		// 处理UTF-8 BOM
		if ((unsigned char)ptr[0] == 0xEF && (end - ptr) >= 3 &&
			(unsigned char)ptr[1] == 0xBB && (unsigned char)ptr[2] == 0xBF) ptr += 3;

		// 解析本行若干列
		double vals[32]; // 足够大
		int colCount = 0; bool ok = true;
		for (; colCount < totalColsPerLine && ptr < end; ++colCount) {
			double v;
			if (!parseFloat(ptr, end, v)) { ok = false; break; }
			vals[colCount] = v;
			while (ptr < end && (*ptr == ' ' || *ptr == '\t')) ++ptr;
			if (ptr < end && (*ptr == '\n' || *ptr == '\r')) {
				if (*ptr == '\r') { ++ptr; if (ptr < end && *ptr == '\n') ++ptr; }
				else ++ptr;
				++colCount; // 视为行结束
				break;
			}
		}
		// 快速跳到行尾
		if (ptr < end && *ptr != '\n') skipToEOL(ptr, end);
		if (!ok) continue;
		if (colCount <= std::max({ xCol,yCol,zCol })) continue;

		const double x = vals[xCol], y = vals[yCol], z = vals[zCol];
		pts.emplace_back(x, y, z);

		if (withN) {
			if (colCount <= std::max({ *nxCol,*nyCol,*nzCol })) {
				// 列数不足，填默认法线
				nrm.emplace_back(0.0, 0.0, 1.0);
			}
			else {
				const double nx = vals[*nxCol], ny = vals[*nyCol], nz = vals[*nzCol];
				gp_Vec v(nx, ny, nz);
				if (v.SquareMagnitude() > 1e-20) nrm.emplace_back(gp_Dir(v));
				else nrm.emplace_back(0.0, 0.0, 1.0);
			}
		}

		if (first) { xmin = xmax = x; ymin = ymax = y; zmin = zmax = z; first = false; }
		else {
			if (x < xmin) xmin = x; else if (x > xmax) xmax = x;
			if (y < ymin) ymin = y; else if (y > ymax) ymax = y;
			if (z < zmin) zmin = z; else if (z > zmax) zmax = z;
		}
	}

	mv.close();

	if (withN) self.SetXYZNAndBBox(std::move(pts), std::move(nrm), xmin, xmax, ymin, ymax, zmin, zmax);
	else       self.SetXYZAndBBox(std::move(pts), xmin, xmax, ymin, ymax, zmin, zmax);

	return true;
}

// 公有重载
bool CloudDataStore::LoadTxtMapped(const std::wstring& path,
	int xCol, int yCol, int zCol,
	std::optional<int> nxCol,
	std::optional<int> nyCol,
	std::optional<int> nzCol,
	int totalColsPerLine)
{
	return loadTxtMappedImpl(path, *this, xCol, yCol, zCol, nxCol, nyCol, nzCol, totalColsPerLine);
}

bool CloudDataStore::LoadTxtMapped(const std::string& path,
	int xCol, int yCol, int zCol,
	std::optional<int> nxCol,
	std::optional<int> nyCol,
	std::optional<int> nzCol,
	int totalColsPerLine)
{
	return loadTxtMappedImpl(path, *this, xCol, yCol, zCol, nxCol, nyCol, nzCol, totalColsPerLine);
}

// —— 自动判别：读取首个非空有效行，统计可解析的浮点数量 ——

// 跳过 BOM / 空白 / 空行 / 注释（# 或 // 开头）
static inline void skipPreamble(const char*& p, const char* end)
{
	// 跳 BOM
	if ((end - p) >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
		p += 3;

	// 跳空行与注释
	for (;;)
	{
		// 跳前导空白
		while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
			// 碰到换行时继续检查下一行
			if (*p == '\n') { ++p; continue; }
			++p;
		}
		if (p >= end) return;

		// 注释行：以 '#' 或 '//' 开头
		if (*p == '#' || ((end - p) >= 2 && p[0] == '/' && p[1] == '/')) {
			while (p < end && *p != '\n') ++p;
			if (p < end && *p == '\n') ++p;
			continue; // 下一行
		}
		break; // 命中首个有效行
	}
}

// 只数首行里可解析的浮点数量（不分配、不复制）
static inline int countFloatsOnLine(const char* p, const char* end)
{
	int cnt = 0;
	const char* q = p;
	for (;;)
	{
		// 行结束？
		if (q >= end || *q == '\n') break;
		if (*q == '\r') { ++q; if (q < end && *q == '\n') ++q; break; }

		// 跳分隔
		while (q < end && (*q == ' ' || *q == '\t')) ++q;
		if (q >= end || *q == '\n') break;

		// 解析一个浮点
		double v;
		auto res = std::from_chars(q, end, v);
		if (res.ec != std::errc()) {
			// 非数字：把这一段丢弃到下一个空白或行尾（容忍垃圾）
			while (q < end && *q != ' ' && *q != '\t' && *q != '\n' && *q != '\r') ++q;
		}
		else {
			++cnt;
			q = res.ptr;
		}
	}
	return cnt;
}

// 自动判别 + 解析（单映射版本）
template<typename PathT>
static bool loadTxtMappedAutoImpl(const PathT& path, CloudDataStore& self)
{
	MappedView mv;
	if (!mapFile(path, mv)) return false;
	const char* beg = mv.data;
	const char* end = mv.data + mv.size;
	const char* ptr = beg;

	// 1) 定位首个有效行，并统计本行可解析数字数量
	skipPreamble(ptr, end);
	if (ptr >= end) { mv.close(); return false; }
	int nFirst = countFloatsOnLine(ptr, end);

	// 经验规则：>=6 视为 XYZ + NX NY NZ；否则按 XYZ
	const bool withN = (nFirst >= 6);

	// 2) 估行数（reserve）
	size_t nLines = 0;
	for (const char* q = beg; q < end; ++q) if (*q == '\n') ++nLines;
	if (nLines == 0) { mv.close(); return false; }

	std::vector<gp_Pnt> pts; pts.reserve(nLines);
	std::vector<gp_Dir> nrm; if (withN) nrm.reserve(nLines);

	// 3) 主解析循环（从文件头重新扫一遍）
	ptr = beg;
	bool first = true;
	double xmin = 0, xmax = 0, ymin = 0, ymax = 0, zmin = 0, zmax = 0;

	while (ptr < end)
	{
		// 跳空行/注释/BOM
		skipPreamble(ptr, end);
		if (ptr >= end) break;

		// 解析一行（容忍列数不足/多余）
		double vals[32];
		int col = 0;
		for (;;)
		{
			// 行/文件结束？
			if (ptr >= end || *ptr == '\n') { if (ptr < end) ++ptr; break; }
			if (*ptr == '\r') { ++ptr; if (ptr < end && *ptr == '\n') ++ptr; break; }

			// 跳分隔
			while (ptr < end && (*ptr == ' ' || *ptr == '\t')) ++ptr;
			if (ptr >= end || *ptr == '\n') { if (ptr < end) ++ptr; break; }
			if (*ptr == '\r') { ++ptr; if (ptr < end && *ptr == '\n') ++ptr; break; }

			// 数字
			double v;
			auto res = std::from_chars(ptr, end, v);
			if (res.ec != std::errc()) {
				// 非数字，跳到下个分隔符或行尾
				while (ptr < end && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r') ++ptr;
				continue;
			}
			if (col < (int)std::size(vals)) vals[col] = v;
			++col;
			ptr = res.ptr;
		}

		// 取 XYZ（前 3 列）
		if (col >= 3) {
			const double x = vals[0], y = vals[1], z = vals[2];
			pts.emplace_back(x, y, z);

			if (withN) {
				if (col >= 6) {
					gp_Vec v(vals[3], vals[4], vals[5]);
					if (v.SquareMagnitude() > 1e-20) nrm.emplace_back(gp_Dir(v));
					else nrm.emplace_back(0.0, 0.0, 1.0);
				}
				else {
					// 本行不够 6 列，给默认法线
					nrm.emplace_back(0.0, 0.0, 1.0);
				}
			}

			if (first) { xmin = xmax = x; ymin = ymax = y; zmin = zmax = z; first = false; }
			else {
				if (x < xmin) xmin = x; else if (x > xmax) xmax = x;
				if (y < ymin) ymin = y; else if (y > ymax) ymax = y;
				if (z < zmin) zmin = z; else if (z > zmax) zmax = z;
			}
		}
		// 若不足 3 列则忽略该行
	}

	mv.close();

	if (pts.empty()) return false;

	if (withN) self.SetXYZNAndBBox(std::move(pts), std::move(nrm), xmin, xmax, ymin, ymax, zmin, zmax);
	else       self.SetXYZAndBBox(std::move(pts), xmin, xmax, ymin, ymax, zmin, zmax);
	return true;
}

// —— 公有自动判别 API ——
bool CloudDataStore::LoadTxtMappedAuto(const std::wstring& path)
{
	return loadTxtMappedAutoImpl(path, *this);
}
bool CloudDataStore::LoadTxtMappedAuto(const std::string& path)
{
	return loadTxtMappedAutoImpl(path, *this);
}