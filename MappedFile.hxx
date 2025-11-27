// MappedFile.hxx
#pragma once
#include <cstddef>
#include <string>

struct MappedView {
	const char* data = nullptr;
	size_t size = 0;
	void close();
};

bool mapFile(const std::wstring& path, MappedView& out); // Windows
bool mapFile(const std::string& path, MappedView& out); // POSIX 也提供窄字串；Windows 同样可用