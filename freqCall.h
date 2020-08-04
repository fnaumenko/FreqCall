#pragma once
#include <iostream>
#include <vector>
#include <chrono>
#include <queue>

using namespace std;
typedef __int64 LLONG;

// 'Point' represents event <time><value> 
struct Point {
	float tm;
	float vl;

	inline Point(float t = 0, float v = 0) : tm(t), vl(v) {}

	// for sorting by descend
	inline bool operator < (const Point& pt) const { return (pt.vl < vl); }

	//static bool CompareDualToDescend(const Point& p1, const Point& p2)
	//{
	//	return VlEq(p1.vl, p2.vl) ? p1.tm < p2.tm : p1.vl > p2.vl;
	//}

	//static bool CompareDualToAscent(const Point& p1, const Point& p2)
	//{
	//	return VlEq(p1.vl, p2.vl) ? p1.tm < p2.tm : p1.vl < p2.vl;
	//}

#ifdef _DEBUG
	void Print() const;
#endif
};


// 'WaveList' represents list of pair <wave file name><frequency>
class WaveList
{
	// 'FileFreq' represents pair <wave file name><frequency>
	struct FileFreq
	{
	private:
		string	_fname;
		float	_freq;

	public:
		// Initializes created instance
		//	@fname: file name
		FileFreq(const string& fname);

		// for sorting by ascent
		inline bool operator < (const FileFreq& f) const { return (f._freq > _freq); }

		void Print() const;
	};

	typedef vector<Point>::const_iterator citer;

	// Simple Moving Average splicer
	class SMA
	{
		// At each step of the slip, the splicer saves the last added value, removes the earliest one,
		// and gives the arithmetic average of the sliding subset length.
		// Thus, the first N-1 steps (where N is the sliding subset length) the splicer does not produce a real output.

		const short	_size;		// sliding subset length
		short		_cnt;		// count of added values
		float		_sum;		// sum of adding values
		queue<float> _q;		// treated value's subset

	public:
		// Constructor
		//	@len: length of base (subset length)
		inline SMA(short len) : _size(len - 1), _cnt(0), _sum(0) {}

		// Add value and return average
		float Push(float x);
	};

	// Simple Moving Median splicer
	class SMM
	{
		// At each step of the slip, the splicer reads the values by first vakue iterator,
		// and gives the median.
		// Thus, at each step, the end of the external value collection is checked.

		const short	_size;		// sliding subset
		const citer	_end;		// end of external collection
		vector<float> _v;		// treated value's subset

	public:
		// Constructor
		//	@halfBase: half of base (subset length)
		//	@end: 'end' iterator of external collection
		inline SMM(short halfBase, citer end) : _size(2 * halfBase + 1), _end(end) { _v.reserve(_size); }

		// Fill subset by first value iterator and return median
		//	it@: an iterator to the first value added
		float Push(citer it);
	};

	const string Ext = "csv";

	vector<FileFreq> _files;	// FileFreq collection

	// Fills instance by file's names found in given directory
	//	@dirName: name of directory
	//	@ext: file's extention as a choosing filter
	//	return: true if files with given extention are found
	bool GetFiles(const string& dirName, const string& ext);

public:
	// Creates and initializes list of paires <wave file name><frequency>
	WaveList(const char* path);

	void Print() const;
};

// 'FileSystem' implements common file system routines
static class FS
{
	// Returns true if file system's object exists
	//	@name: object's name
	//	@st_mode: object's system mode
	static bool IsExist(const char* name, int st_mode);

public:
	// Gets size of file or -1 if file doesn't exist
	static LLONG Size(const char*);

	// Returns true if directory exists
	inline static bool IsDirExist(const char* name) { return IsExist(name, S_IFDIR); }

	// Returns the name ended by slash without checking
	static string const MakePath(const string& name);

} fs;

// 'Timer' measures the single wall time interval
class Timer
{
	chrono::steady_clock::time_point _begin;
	bool _enable;

public:
	// Timer constructor
	// @enable: true if timer sould be in active state
	inline Timer(bool enable) : _enable(enable) {}

	// Starts timer if enabled
	inline void Start() { if (_enable) _begin = std::chrono::steady_clock::now(); }

	// Stops timer and prints elapsed time, if enabled
	void Stop();
};
