#pragma once
#include <iostream>
#include <vector>
#include <chrono>
#include <queue>

using namespace std;

const char LF = '\n';


// 'WaveList' represents list of pair <wave file name><frequency>
class WaveList
{
	class File_freq
	{
		string	_fname;
		float	_freq;

	public:
		// Initializes the created instance
		//	@fname: file name
		File_freq(const string& fname);

		// for sorting by ascent
		inline bool operator < (const File_freq& f) const { return (f._freq > _freq); }

		inline void Print() const { cout << _fname << '\t' << (_freq/1000) << LF; }
	};

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
		//	@halfBase: half of base (subset length)
		inline SMA(short halfBase) : _size(2 * halfBase + 1), _cnt(0), _sum(0) {}

		// Add value and return average
		float Push(float x);
	};


	const string ext = "csv";

	vector<File_freq> files;

	// Fills instance by file's names found in given directory
	//	@dirName: name of directory
	//	@ext: file's extention as a choosing filter
	//	return: true if files with given extention are found
	bool GetFiles(const string& dirName, const string& ext);

public:
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
	// Returns the name ended by slash without checking
	static string const MakePath(const string& name);

	// Returns true if directory exists
	inline static bool IsDirExist(const char* name) { return IsExist(name, S_IFDIR); }
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
