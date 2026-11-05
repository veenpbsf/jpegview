#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#define NOMINMAX
#include "../src/JPEGView/ExplorerOrder.h"
#include <iostream>
#include <cstdlib>

// Read-only probe. The optional HWND lets an integration test target its own
// Explorer window; normal usage captures the current foreground window.
int wmain(int argc, wchar_t** argv) {
	if (argc < 2) {
		std::wcerr << L"Usage: ExplorerOrderProbe.exe <image-path> [Explorer-HWND-decimal] [delay-poll-ms]\n";
		return 2;
	}
	HWND source = argc > 2 ? reinterpret_cast<HWND>(static_cast<ULONG_PTR>(_wcstoui64(argv[2], nullptr, 10))) : GetForegroundWindow();
	CExplorerOrderSnapshot query;
	if (!query.Start(argv[1], source)) { std::cout << "UNAVAILABLE:start\n"; return 3; }
	if (argc > 3) Sleep(static_cast<DWORD>(_wtoi(argv[3])));
	std::vector<std::wstring> paths;
	CExplorerOrderSnapshot::Status status;
	do { status = query.Poll(paths); if (status == CExplorerOrderSnapshot::Pending) Sleep(10); }
	while (status == CExplorerOrderSnapshot::Pending);
	if (status != CExplorerOrderSnapshot::Ready) { std::cout << "UNAVAILABLE\n"; return 3; }
	for (const auto& path : paths) std::wcout << path << '\n';
	return 0;
}
