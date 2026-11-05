#pragma once

#include <unordered_map>

// Hash and lexical ordering of LPCTSTR string contents.
class CHashCompareLPCTSTR
{
public:
	static const size_t bucket_size = 4;
	static const size_t min_buckets = 8;
	CHashCompareLPCTSTR() {}
	size_t operator( )(const LPCTSTR& Key) const;
	bool operator( )(const LPCTSTR& _Key1, const LPCTSTR& _Key2) const;
};
// unordered_map requires equality, whereas the legacy comparator supplies ordering.
// Compare string contents rather than the addresses held by LPCTSTR.
struct CStringKeyEqual
{
    bool operator()(LPCTSTR left, LPCTSTR right) const
    {
        return _tcscmp(left, right) == 0;
    }
};

template<typename Value>
using CStringHashMap = std::unordered_map<LPCTSTR, Value, CHashCompareLPCTSTR, CStringKeyEqual>;