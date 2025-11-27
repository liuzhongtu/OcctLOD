// MappedFile.cxx

#include "pch.h"
#include "MappedFile.hxx"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

void MappedView::close() {
#ifdef _WIN32
	if (data) UnmapViewOfFile(data);
#else
	if (data) munmap((void*)data, size);
#endif
	data = nullptr; size = 0;
}

#ifdef _WIN32
bool mapFile(const std::wstring& path, MappedView& out) {
	HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) return false;
	LARGE_INTEGER sz; if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0) { CloseHandle(hFile); return false; }
	HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!hMap) { CloseHandle(hFile); return false; }
	void* p = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	CloseHandle(hMap); CloseHandle(hFile);
	if (!p) return false;
	out.data = static_cast<const char*>(p);
	out.size = static_cast<size_t>(sz.QuadPart);
	return true;
}
bool mapFile(const std::string& path, MappedView& out) {
	int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
	std::wstring w; w.resize(wlen ? wlen - 1 : 0);
	if (wlen) MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w.data(), wlen);
	return mapFile(w, out);
}
#else
bool mapFile(const std::string& path, MappedView& out) {
	int fd = ::open(path.c_str(), O_RDONLY);
	if (fd < 0) return false;
	struct stat st; if (fstat(fd, &st) != 0 || st.st_size <= 0) { close(fd); return false; }
	void* p = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (p == MAP_FAILED) return false;
	out.data = static_cast<const char*>(p);
	out.size = static_cast<size_t>(st.st_size);
	return true;
}
bool mapFile(const std::wstring& /*path*/, MappedView& /*out*/) { return false; } // 非 Windows 不实现
#endif