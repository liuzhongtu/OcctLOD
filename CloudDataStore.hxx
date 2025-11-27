#pragma once
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Real.hxx>
#include <vector>
#include <optional>
#include <string>

// SoA 视图：仅保存指针和大小，不拥有数据
struct CloudSoAView
{
	const Standard_Real* X = nullptr;
	const Standard_Real* Y = nullptr;
	const Standard_Real* Z = nullptr;

	const Standard_Real* NX = nullptr;
	const Standard_Real* NY = nullptr;
	const Standard_Real* NZ = nullptr;

	size_t Size = 0;

	bool HasNormals() const { return NX != nullptr && NY != nullptr && NZ != nullptr; }
	bool Empty()     const { return Size == 0; }
};

class CloudDataStore {
public:
	// ---- 基本信息 ----
	size_t Size() const { return P_.size(); }
	const Bnd_Box& BBox() const { return BndAll_; }

	// ---- 点坐标 ----
	const std::vector<gp_Pnt>& Points() const { return P_; }
	std::vector<gp_Pnt>& Points() { return P_; }

	// ---- 法向量 ----
	bool HasNormals() const { return !N_.empty(); }
	const std::vector<gp_Dir>& Normals() const { return N_; }
	const gp_Dir* NormalPtrOrNull(size_t i) const { return (i < N_.size()) ? &N_[i] : nullptr; }

	// ---- SoA 视图：按需懒构建 ----
	CloudSoAView SoA() const;

	// ---- 设置：XYZ / XYZ+N，自动重算 bbox ----
	void SetXYZ(std::vector<gp_Pnt> pts);
	void SetXYZN(std::vector<gp_Pnt> pts, std::vector<gp_Dir> nrm);

	// ---- 设置：调用方已经给好 bbox，用于加载优化 ----
	void SetXYZAndBBox(std::vector<gp_Pnt> pts,
		Standard_Real xmin, Standard_Real xmax,
		Standard_Real ymin, Standard_Real ymax,
		Standard_Real zmin, Standard_Real zmax);
	void SetXYZNAndBBox(std::vector<gp_Pnt> pts, std::vector<gp_Dir> nrm,
		Standard_Real xmin, Standard_Real xmax,
		Standard_Real ymin, Standard_Real ymax,
		Standard_Real zmin, Standard_Real zmax);

	// ---- 自适应自动识别是否有法向 ----
	bool LoadTxtMappedAuto(const std::wstring& path);
	bool LoadTxtMappedAuto(const std::string& path);

	// ---- 文本映射加载 ----
	bool LoadTxtMapped(const std::wstring& path,
		int xCol = 0, int yCol = 1, int zCol = 2,
		std::optional<int> nxCol = {},
		std::optional<int> nyCol = {},
		std::optional<int> nzCol = {},
		int totalColsPerLine = 3);

	bool LoadTxtMapped(const std::string& path,
		int xCol = 0, int yCol = 1, int zCol = 2,
		std::optional<int> nxCol = {},
		std::optional<int> nyCol = {},
		std::optional<int> nzCol = {},
		int totalColsPerLine = 3);

private:
	void computeBBox_();

	// SoA 内部维护：只在需要时从 AoS(P_/N_) 构建一次
	void invalidateSoA_();
	void rebuildSoA_() const;

private:
	// AoS 存储
	std::vector<gp_Pnt> P_;   // 点
	std::vector<gp_Dir> N_;   // 法向，可空：size()==0

	// SoA 缓冲区（mutable：按需懒构建）
	mutable std::vector<Standard_Real> X_;
	mutable std::vector<Standard_Real> Y_;
	mutable std::vector<Standard_Real> Z_;
	mutable std::vector<Standard_Real> NX_;
	mutable std::vector<Standard_Real> NY_;
	mutable std::vector<Standard_Real> NZ_;
	mutable bool soaDirty_ = true;

	Bnd_Box BndAll_;
};