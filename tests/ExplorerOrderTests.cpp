#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#define NOMINMAX
#include "../src/JPEGView/ExplorerOrderPolicy.h"
#include "../src/JPEGView/ExplorerOrder.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
	struct Entry {
		std::wstring path;
		explicit Entry(std::wstring value) : path(std::move(value)) {}
		Entry(const Entry&) = delete; // the production descriptors must not be copied
		Entry& operator=(const Entry&) = delete;
	};
	void Require(bool condition, const char* message) {
		if (!condition) throw std::runtime_error(message);
	}
	std::vector<std::wstring> Paths(const std::list<Entry>& entries) {
		std::vector<std::wstring> result;
		for (const auto& entry : entries) result.push_back(entry.path);
		return result;
	}
	void Reorder(std::list<Entry>& entries, const std::vector<std::wstring>& snapshot) {
		ExplorerOrderPolicy::Reorder(entries, snapshot, [](const Entry& e) { return e.path; });
	}
}

int main() {
	try {
		// Arbitrary view order, selected image in the middle, stable node identity.
		std::list<Entry> files;
		files.emplace_back(L"C:\\photos\\a.jpg");
		files.emplace_back(L"C:\\photos\\b.jpg");
		files.emplace_back(L"C:\\photos\\c.jpg");
		auto selected = std::next(files.begin());
		Entry* selectedAddress = &*selected;
		Reorder(files, {L"C:\\photos\\c.jpg", L"C:\\photos\\b.jpg", L"C:\\photos\\a.jpg"});
		Require(Paths(files) == std::vector<std::wstring>{L"C:\\photos\\c.jpg", L"C:\\photos\\b.jpg", L"C:\\photos\\a.jpg"}, "view order");
		Require(&*selected == selectedAddress && selected->path == L"C:\\photos\\b.jpg", "selected image/iterator survived");
		Require(std::prev(selected)->path == L"C:\\photos\\c.jpg" && std::next(selected)->path == L"C:\\photos\\a.jpg", "previous and next");
		Require(files.front().path == L"C:\\photos\\c.jpg" && files.back().path == L"C:\\photos\\a.jpg", "home and end");

		// Duplicate paths, missing/deleted entries, Unicode and path casing.
		std::list<Entry> unicode;
		unicode.emplace_back(L"C:\\Pictures\\\u732b 2.PNG");
		unicode.emplace_back(L"C:\\Pictures\\\u732b 1.PNG");
		unicode.emplace_back(L"C:\\Pictures\\new-A.PNG");
		unicode.emplace_back(L"C:\\Pictures\\new-B.PNG");
		Reorder(unicode, {L"c:\\pictures\\\u732b 1.png", L"C:\\Pictures\\deleted.PNG",
			L"C:\\Pictures\\\u732b 1.PNG", L"c:\\pictures\\\u732b 2.png"});
		Require(Paths(unicode) == std::vector<std::wstring>{L"C:\\Pictures\\\u732b 1.PNG", L"C:\\Pictures\\\u732b 2.PNG", L"C:\\Pictures\\new-A.PNG", L"C:\\Pictures\\new-B.PNG"}, "case/Unicode/duplicates/deletions/additions");
		auto unchanged = Paths(unicode);
		Reorder(unicode, {});
		Require(Paths(unicode) == unchanged, "empty snapshot preserves fallback");
		std::list<Entry> empty;
		Reorder(empty, {L"C:\\missing.jpg"});
		Require(empty.empty(), "empty folder");

		CExplorerOrderSnapshot query;
		std::vector<std::wstring> output;
		Require(!query.Start(L"C:\\missing.jpg", nullptr), "missing origin falls back");
		Require(!query.Start(L"C:\\missing.jpg", GetDesktopWindow()), "desktop excluded");
		Require(query.Poll(output) == CExplorerOrderSnapshot::Unavailable, "no query pending");
		query.Cancel();
		query.Cancel();
		std::cout << "PASS: view order, selected node, previous/next, home/end, Unicode, case, duplicates, deleted/new files, empty lists and no-origin fallback.\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
