// JPEGView Explorer-order extension. GPL-2.0-or-later, like JPEGView.
// This translation unit intentionally does not use the XP-targeting PCH.
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#define NOMINMAX
#include "ExplorerOrder.h"
#include <shlobj.h>
#include <exdisp.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <servprov.h>
#include <atomic>
#include <thread>
#include <utility>
#ifdef JPEGVIEW_EXPLORER_ORDER_DIAGNOSTICS
#include <iostream>
#define EXPLORER_TRACE(value) (std::wcerr << L"[ExplorerOrder] " << value << L'\n')
#else
#define EXPLORER_TRACE(value) ((void)0)
#endif

#ifdef _MSC_VER
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

namespace {
	const DWORD QueryBudgetMs = 2000;
	const size_t MaxViewItems = 100000;
	std::atomic<unsigned> ActiveReaders{0};
	struct ReleaseReaderSlot {
		~ReleaseReaderSlot() { ActiveReaders.fetch_sub(1, std::memory_order_relaxed); }
	};

	template<class T> class ComPtr {
	public:
		ComPtr() : p(nullptr) {}
		~ComPtr() { if (p) p->Release(); }
		ComPtr(const ComPtr&) = delete;
		ComPtr& operator=(const ComPtr&) = delete;
		T* operator->() const { return p; }
		explicit operator bool() const { return p != nullptr; }
		T** Put() { return &p; } // only use once, on an empty pointer
		void** PutVoid() { return reinterpret_cast<void**>(&p); }
	private:
		T* p;
	};

	struct FreeTaskMemory {
		void operator()(ITEMIDLIST* p) const { CoTaskMemFree(p); }
	};
	using Pidl = std::unique_ptr<ITEMIDLIST, FreeTaskMemory>;

	bool SamePath(const std::wstring& a, const std::wstring& b) {
		return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
	}

	std::wstring NormalizePath(const std::wstring& path) {
		// The upstream viewer uses MAX_PATH buffers in several places. Do not
		// pretend this extension adds extended-length or virtual-path support.
		if (path.empty() || path.size() >= MAX_PATH) return {};
		wchar_t full[MAX_PATH];
		DWORD length = GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr);
		if (!length || length >= MAX_PATH) return {};
		std::wstring result(full, length);
		for (auto& c : result) if (c == L'/') c = L'\\';
		while (result.size() > 3 && result.back() == L'\\') result.pop_back();
		return result;
	}

	std::wstring ParentPath(const std::wstring& path) {
		auto slash = path.find_last_of(L'\\');
		if (slash == std::wstring::npos) return {};
		return path.substr(0, slash == 2 && path[1] == L':' ? 3 : slash);
	}

	bool IsExplorerWindow(HWND window) {
		if (!window || !IsWindow(window)) return false;
		wchar_t name[64] = {};
		GetClassNameW(window, name, 64);
		return wcscmp(name, L"CabinetWClass") == 0 || wcscmp(name, L"ExploreWClass") == 0;
	}

	bool IsActiveView(HWND view, HWND root) {
		// Inspect the tab/view hierarchy, not the root window's visibility. The
		// originating Explorer may have been minimized/hidden after launching us.
		for (HWND child = view; child && child != root; child = GetParent(child)) {
			if (!(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE)) return false;
		}
		return view && GetAncestor(view, GA_ROOT) == root;
	}

	bool GetItemPath(IShellFolder* folder, PCUITEMID_CHILD item, std::wstring& path) {
		SFGAOF attributes = SFGAO_FILESYSTEM | SFGAO_FOLDER;
		if (FAILED(folder->GetAttributesOf(1, &item, &attributes)) ||
			!(attributes & SFGAO_FILESYSTEM) || (attributes & SFGAO_FOLDER)) return false;
		STRRET name = {};
		if (FAILED(folder->GetDisplayNameOf(item, SHGDN_FORPARSING, &name))) return false;
		PWSTR text = nullptr;
		if (FAILED(StrRetToStrW(&name, item, &text))) return false;
		path = NormalizePath(text ? text : L"");
		CoTaskMemFree(text);
		return !path.empty();
	}
}

struct CExplorerOrderSnapshot::State {
	std::wstring image;
	std::wstring folder;
	HWND sourceWindow;
	DWORD started;
	std::atomic<bool> cancelled{false};
	std::atomic<Status> status{Pending};
	std::vector<std::wstring> paths;

	bool Expired() const {
		return cancelled.load(std::memory_order_relaxed) ||
			static_cast<DWORD>(GetTickCount() - started) >= QueryBudgetMs;
	}
};

CExplorerOrderSnapshot::~CExplorerOrderSnapshot() { Cancel(); }

void CExplorerOrderSnapshot::Cancel() {
	if (m_state) m_state->cancelled.store(true, std::memory_order_relaxed);
	m_state.reset(); // never joins a worker stuck inside an Explorer COM call
}

bool CExplorerOrderSnapshot::Start(const std::wstring& imagePath, HWND sourceWindow) {
	Cancel();
	HWND root = sourceWindow ? GetAncestor(sourceWindow, GA_ROOT) : nullptr;
	if (!IsExplorerWindow(root)) return false;
	const auto image = NormalizePath(imagePath);
	const auto folder = ParentPath(image);
	DWORD attributes = image.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(image.c_str());
	if (folder.empty() || attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
	try {
		auto state = std::make_shared<State>();
		state->image = image;
		state->folder = folder;
		state->sourceWindow = root;
		state->started = GetTickCount();
		// A stuck out-of-process COM server cannot accumulate unbounded workers
		// if many images are opened in the same viewer process.
		unsigned active = ActiveReaders.load(std::memory_order_relaxed);
		while (active < 2 && !ActiveReaders.compare_exchange_weak(active, active + 1)) {}
		if (active >= 2) return false;
		try {
			std::thread worker([state]() {
				ReleaseReaderSlot release;
				Read(state);
			});
			worker.detach();
		} catch (...) {
			ActiveReaders.fetch_sub(1, std::memory_order_relaxed);
			return false;
		}
		m_state = std::move(state);
		return true;
	} catch (...) {
		return false; // failure to create a thread must not stop image viewing
	}
}

CExplorerOrderSnapshot::Status CExplorerOrderSnapshot::Poll(std::vector<std::wstring>& paths) {
	if (!m_state) return Unavailable;
	Status status = m_state->status.load(std::memory_order_acquire);
	if (status == Pending) {
		if (m_state->Expired()) { Cancel(); return Unavailable; }
		return Pending;
	}
	// A timely completed snapshot stays valid even if decoding/painting the
	// initial image kept the UI busy for longer than the query budget.
	if (status == Ready) paths.swap(m_state->paths);
	m_state.reset();
	return status;
}

void CExplorerOrderSnapshot::Read(const std::shared_ptr<State>& state) {
	EXPLORER_TRACE(L"worker started, source=" << state->sourceWindow);
	HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(initialized)) {
		state->status.store(Unavailable, std::memory_order_release);
		return;
	}
	bool success = false;
	try {
		// Keep every COM pointer inside this scope, releasing it before CoUninitialize.
		ComPtr<IShellWindows> windows;
		HRESULT created = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER,
			IID_IShellWindows, windows.PutVoid());
		EXPLORER_TRACE(L"ShellWindows HRESULT=" << std::hex << created << std::dec);
		if (SUCCEEDED(created)) {
			long count = 0;
			if (SUCCEEDED(windows->get_Count(&count))) {
				EXPLORER_TRACE(L"windows=" << count);
				for (long i = 0; i < count && !state->Expired() && !success; ++i) {
					VARIANT index;
					VariantInit(&index);
					index.vt = VT_I4;
					index.lVal = i;
					ComPtr<IDispatch> dispatch;
					if (FAILED(windows->Item(index, dispatch.Put())) || !dispatch) continue;
					ComPtr<IWebBrowser2> web;
					if (FAILED(dispatch->QueryInterface(IID_IWebBrowser2, web.PutVoid()))) continue;
					SHANDLE_PTR window = 0;
					HRESULT windowResult = web->get_HWND(&window);
					EXPLORER_TRACE(L"candidate=" << reinterpret_cast<HWND>(window));
					if (FAILED(windowResult) || GetAncestor(reinterpret_cast<HWND>(window), GA_ROOT) != state->sourceWindow) continue;
					ComPtr<IServiceProvider> services;
					if (FAILED(dispatch->QueryInterface(IID_IServiceProvider, services.PutVoid()))) continue;
					ComPtr<IShellBrowser> browser;
					HRESULT browserResult = services->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, browser.PutVoid());
					EXPLORER_TRACE(L"browser HRESULT=" << std::hex << browserResult << std::dec);
					if (FAILED(browserResult)) continue;
					ComPtr<IShellView> view;
					if (FAILED(browser->QueryActiveShellView(view.Put()))) continue;
					HWND viewWindow = nullptr;
					HRESULT viewResult = view->GetWindow(&viewWindow);
					EXPLORER_TRACE(L"view=" << viewWindow << L" visible=" << IsWindowVisible(viewWindow));
					if (FAILED(viewResult) || !IsActiveView(viewWindow, state->sourceWindow)) continue;
					ComPtr<IFolderView> folderView;
					if (FAILED(view->QueryInterface(IID_IFolderView, folderView.PutVoid()))) continue;
					ComPtr<IPersistFolder2> persisted;
					if (FAILED(folderView->GetFolder(IID_IPersistFolder2, persisted.PutVoid()))) continue;
					PIDLIST_ABSOLUTE rawFolder = nullptr;
					if (FAILED(persisted->GetCurFolder(&rawFolder)) || !rawFolder) continue;
					Pidl folderId(rawFolder);
					wchar_t folderPath[MAX_PATH] = {};
					BOOL pathResult = SHGetPathFromIDListW(folderId.get(), folderPath);
					EXPLORER_TRACE(L"folder=" << folderPath << L" expected=" << state->folder);
					if (!pathResult ||
						!SamePath(NormalizePath(folderPath), state->folder)) continue; // excludes search/library views
					ComPtr<IShellFolder> folder;
					if (FAILED(folderView->GetFolder(IID_IShellFolder, folder.PutVoid()))) continue;
					ComPtr<IEnumIDList> items;
					HRESULT itemsResult = folderView->Items(SVGIO_ALLVIEW | SVGIO_FLAG_VIEWORDER,
						IID_IEnumIDList, items.PutVoid());
					EXPLORER_TRACE(L"items HRESULT=" << std::hex << itemsResult << std::dec);
					if (FAILED(itemsResult)) continue;
					std::vector<std::wstring> paths;
					bool foundImage = false;
					bool complete = false;
					while (!state->Expired() && paths.size() < MaxViewItems) {
						PITEMID_CHILD rawItem = nullptr;
						HRESULT next = items->Next(1, &rawItem, nullptr);
						Pidl item(rawItem);
						if (next == S_FALSE) { complete = true; break; }
						if (next != S_OK || !rawItem) break;
						std::wstring path;
						if (!GetItemPath(folder.operator->(), item.get(), path) ||
							!SamePath(ParentPath(path), state->folder)) continue;
						foundImage = foundImage || SamePath(path, state->image);
						paths.push_back(std::move(path));
					}
					// Never publish a partial enumeration as the supposedly complete order.
					EXPLORER_TRACE(L"paths=" << paths.size() << L" complete=" << complete << L" found=" << foundImage << L" expired=" << state->Expired());
					if (complete && foundImage && !state->Expired()) {
						state->paths.swap(paths);
						success = true;
					}
				}
			}
		}
	} catch (...) { success = false; }
	CoUninitialize();
	EXPLORER_TRACE(L"worker finished, success=" << success << L" elapsed=" << static_cast<DWORD>(GetTickCount() - state->started));
	state->status.store(success ? Ready : Unavailable, std::memory_order_release);
}
