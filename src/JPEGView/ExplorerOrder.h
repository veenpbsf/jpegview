#pragma once

#include <windows.h>
#include <memory>
#include <string>
#include <vector>

// Reads a one-time snapshot on a private COM STA. No Shell COM call runs on the
// UI thread, and the worker never keeps a dialog/window-object pointer.
class CExplorerOrderSnapshot {
public:
	enum Status { Pending, Ready, Unavailable };
	CExplorerOrderSnapshot() = default;
	~CExplorerOrderSnapshot();
	CExplorerOrderSnapshot(const CExplorerOrderSnapshot&) = delete;
	CExplorerOrderSnapshot& operator=(const CExplorerOrderSnapshot&) = delete;

	// sourceWindow must be captured before JPEGView takes foreground focus.
	bool Start(const std::wstring& imagePath, HWND sourceWindow);
	Status Poll(std::vector<std::wstring>& paths);
	bool IsPending() const { return m_state != nullptr; }
	void Cancel();

private:
	struct State;
	std::shared_ptr<State> m_state;
	static void Read(const std::shared_ptr<State>& state);
};
