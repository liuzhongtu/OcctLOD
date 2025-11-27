#pragma once

struct LodTrigger
{
	// —— 配置 ——
	UINT  timerId = 0;    // WM_TIMER id（外部自定，例如 1001）
	DWORD debounceMs = 100;  // 防抖间隔（120~200ms常用）

	// —— 运行时状态 ——
	bool  dirty = false; // 视图变化影响LOD？
	UINT  runningTimer = 0;     // SetTimer返回的实际timer句柄
	DWORD lastTick = 0;     // 最近一次交互时间戳 GetTickCount()

	// 标记“LOD 需要更新”，并重置防抖计时器
	inline void Mark(HWND hwnd)
	{
		dirty = true;
		lastTick = GetTickCount();
		if (runningTimer) KillTimer(hwnd, runningTimer);
		runningTimer = SetTimer(hwnd, timerId, debounceMs, nullptr);
	}

	// 在 OnTimer 中调用；若到达防抖间隔，返回 true（可触发 LOD 计算）
	inline bool OnTimer(HWND hwnd, UINT_PTR id)
	{
		if (id != timerId) return false;
		const DWORD now = GetTickCount();
		if (now - lastTick >= debounceMs)
		{
			if (runningTimer) { KillTimer(hwnd, runningTimer); runningTimer = 0; }
			if (dirty) { dirty = false; return true; }
			return false;
		}
		// 还没到间隔，继续等
		if (runningTimer) { KillTimer(hwnd, runningTimer); }
		runningTimer = SetTimer(hwnd, timerId, debounceMs, nullptr);
		return false;
	}

	// 立即触发一次（跳过防抖），常用于首帧或强制刷新
	inline void FireNow() { dirty = false; lastTick = GetTickCount(); }
};
