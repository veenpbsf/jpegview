#pragma once
#include <cwchar>
#include <list>
#include <map>
#include <string>
#include <vector>

namespace ExplorerOrderPolicy {
	struct PathLess {
		bool operator()(const std::wstring& a, const std::wstring& b) const {
			// Match CFileList::FindFile's case-insensitive CRT path semantics.
			return _wcsicmp(a.c_str(), b.c_str()) < 0;
		}
	};

	// Move existing nodes, never reconstruct file descriptors or invalidate their
	// iterators. Items absent from the opening snapshot remain at the end, in
	// their existing order (e.g. newly created files on a later directory reload).
	template<class Entry, class GetPath>
	void Reorder(std::list<Entry>& files, const std::vector<std::wstring>& snapshot, GetPath getPath) {
		std::map<std::wstring, typename std::list<Entry>::iterator, PathLess> index;
		for (auto i = files.begin(); i != files.end(); ++i) index.emplace(getPath(*i), i);
		std::list<Entry> ordered;
		for (const auto& path : snapshot) {
			auto found = index.find(path);
			if (found == index.end()) continue;
			ordered.splice(ordered.end(), files, found->second);
			index.erase(found); // duplicate entries in a Shell result are harmless
		}
		ordered.splice(ordered.end(), files);
		files.swap(ordered);
	}
}
