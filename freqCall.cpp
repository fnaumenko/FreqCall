/************************************************************************************
freqCall - frequency recognition - recognition of frequencies recorded in files.
This is test for Pico Technology

Fedor Naumenko (fedor.naumenko@gmail.com), 04.08.2020

Synopsis:
	freqCall [-t] [-h] [path]
		-t: print elapsed time
		-h: print help and quit
		path: directory where the wave files are located
************************************************************************************/

#include "freqCall.h"
#include <windows.h>
#include <sys/stat.h>	// struct stat
#include <fstream>
#include <sstream>
#include <algorithm>	// std::replace, std::sort
#include <iomanip>      // std::setw
#include <locale>		// std::locale
#include <cfloat>       // FLT_MAX

const char LF = '\n';

int main(int argc, char* argv[]) {
	bool setTimer = false;
	char* par = NULL;
	// *** parsing parameters ***
	for (int i=1; i<argc; i++) {
		char* opt = argv[i];
		if (opt[0] == '-') {
			if (opt[1] == 't')		setTimer = true;
			//else if (opt[1] == 'l')	cout.imbue(locale("English"));
			else if (opt[1] == 'h') {
				cout << "frequency call: calculates the frequency in wave files.\n"
					<< "freqCall [-t] [-h] [path]\n"
					<< " where\t-t: print elapsed time\n"
					<< "\t-h: print help and quit\n"
					<< "\tpath: directory where the wave files are located; default '.'\n";
				return 0;
			}
		}
		else par = opt;
	}
	const char* path = par ? par : ".";
	if (!FS::IsDirExist(path)) { cerr << "directory '" << path << "' does not exist\n"; return 1; }
	int ret = 0;
	Timer timer(setTimer);

	// *** execution ***
	timer.Start();
	try {
		WaveList list(path);
		list.Print();
	}
	catch (const exception& e) { ret = 1; cerr << e.what() << endl; }
	timer.Stop();
	return ret;
}


/************************ class WaveList ************************/
//	Add value and return average
float WaveList::SMA::Push(float x)
{
	_sum += x;
	_q.push(x);
	if (_cnt == _size) {		// is the queue full?
		_sum -= _q.front();		// reduce the amount by the first value; Complexity: constant 
		_q.pop();				// remove first value; Complexity: constant 
		return (float)_sum / _cnt;
	}
	_cnt++;
	return FLT_MIN;
}

// Fill subset by first value and return median
//	it@: an iterator to the first value added
float WaveList::SMM::Push(citer it)
{
	_v.clear();
	for (short i = 0; i < _size; i++) {
		_v.push_back(it->vl);
		if (++it == _end)
			return FLT_MIN;
	}
	sort(_v.begin(), _v.end());
	return _v[_size / 2];				// value in the middle
}

//// Returns true if 2 floats are equal to epsilon
//bool fless(float a, float b, const float epsilon = 0.00001) { return a < b + epsilon; }

// Returns true if 2 floats are equal to epsilon
inline bool fEq(float a, float b, const float epsilon) { return fabs(a - b) < epsilon; }

inline bool VlEq(float a, float b) { return fEq(a, b, 0.0001f); }
//inline bool TmEq(float a, float b) { return fEq(a, b, 1e-8); }


#ifdef _DEBUG
inline void Point::Print() const { cout << setw(10) << tm << ": " << setw(10) << left << vl; }
#endif

// Extract values from line
//	return: true if the values could not be retrieved, false if alright
bool ParseLine(const string& line, Point& pt)
{
	istringstream iss(line);
	if (!(iss >> pt.tm))	return true;
	iss.get();				// skip comma
	if (!(iss >> pt.vl))	return true;
	return false;
}

// Returns true if two floats have diferent sign
inline bool DiffSign(float a, float b) { return a * b < 0.0f; }

// Initializes the created instance
//	@fname: file name
WaveList::FileFreq::FileFreq(const string& fname)	: _fname(fname), _freq(0)
{
	fstream fs(fname , ios::in);
	if (!fs.is_open())			return;

	const int AR_Part = 3;	// minimum part of amplitude range defining a jump 
	int ptCnt = 0;			// approximate number of points in the period
	int n = 0;				// full period counter, general counter
	float prev_vl = FLT_MIN;// previous time, value
	float vl, sum_tm = 0;	// current value (volts), time sum
	float tolerance;		// the size of the error,
							// above which the difference between points is taken into account
	vector<Point> pts;		// time-value collection from file
	bool jumped = false;	// true if signal is jumping
	bool noisy;				// true if signal is noisy
	bool suppos = false;	// true if signal is superposition of waves of different frequencies (modulated)
	char sFactor = 1;		// sign factor: if 1 then counting summits (at the beginning of the fall),
							// if -1, then counting bottoms (at the beginning of the rise)

	// *** read signal from file and define signal signs and factors
	// The block can be formatted as a method with 5 in-out parameters 
	// (when combining three boolean variables into one bitset)
	{
		float prev_vl = FLT_MIN;	// previous time; some problrms with compilation using std::numeric_limits
		float min_vl = FLT_MAX;		// current min value
		float max_vl = -FLT_MAX;	// current max value
		float prev_diff = -FLT_MAX;	// previous difference between closest values
		float min_tm = 0;			// time of first min value
		float diff = 0;				// difference between lower and upper values
		bool countMax = true;		// true if maximum are counted
		int diffSignCnt = 0;		// counter of different sign cases
		string line;				// current read line
		Point pt;

		// *** fill time-value collection and define if it's noisy
		pts.reserve(50);
		getline(fs, line);			// skip title
		while (getline(fs, line)) {
			if (ParseLine(line, pt))	return;
			pts.push_back(pt);

			if (prev_vl != FLT_MAX) {
				diff = prev_vl - pt.vl;
				if (ptCnt++ < 10) {
					if (prev_diff != -FLT_MAX)
						diffSignCnt += DiffSign(diff, prev_diff);
					prev_diff = diff;
				}
			}
			prev_vl = pt.vl;
		}
		noisy = diffSignCnt > 2;

		// the fisrt part of the collection scanned to determine the parameters 
		const int ScanedPART = pts.size() > 1000 ? 4 : 2;
		citer it_end = pts.begin() + pts.size() / ScanedPART;	// end of first part
		// *** define max, min vals within first part
		for (auto it = pts.begin(); it != it_end; it++)
			if (it->vl >= max_vl) {
				if (!countMax)		min_tm = it->tm;
				max_vl = it->vl;
			}
			else if (it->vl <= min_vl) {
				min_vl = it->vl;
				min_tm = it->tm;
				countMax = false;
			}
		
		// *** define if signal is jumping
		tolerance = max_vl - min_vl;
		float critDiff = tolerance / AR_Part;
		prev_vl = pts[1].vl;
		for (auto it = pts.begin() + 2; it != it_end; it++) {
			diff = prev_vl - it->vl;
			if (fabs(diff) > critDiff) {
				jumped = true;
				if(diff < 0)	sFactor = -1;
				break;
			}
			prev_vl = it->vl;
		}
		ptCnt = min_tm / (pts[1].tm - pts[0].tm);

		// *** define if signal is frequency overlay
		if (!jumped && !noisy) {
			prev_vl = pts[0].vl;
			for (citer it = pts.begin() + 1; it != it_end; it++) {
				diff = prev_vl - it->vl;
				if (it->vl < max_vl && diff > 0) {
					suppos = true;
					break;
				}
				if(VlEq(it->vl, max_vl))	break;
				prev_vl = it->vl;
			}
		}
		tolerance /= AR_Part;
		if (!jumped)	tolerance /= ptCnt;
#ifdef _DEBUG
		cout << fname << "  noisy: " << (noisy ? "YES" : "NO ") << "  jumped: " << (jumped ? "YES" : "NO ")
			<< "  superpos: " << (suppos ? "YES" : "NO ") << " points: " << ptCnt << "  min_tm: " << min_tm
			<< "  tolerance: " << tolerance << LF;
#endif
	}

	// *** calculate frequency
	SMA sma(pts.size() > 2000 || suppos ? 20 : 4);				// Average splicer
	SMM smm(noisy ? ptCnt / 40 : (suppos ? 7 : 1), pts.cend());	// Median splicer
	float tm0 = 0;		// start counting time
	bool splice = suppos || (noisy && !jumped);
	bool grow = true;	// if true then signal is growing
	
	n = 0;
	prev_vl = pts[0].vl;
	if(splice)	sma.Push(smm.Push(pts.begin()));
	for (auto it = pts.begin() + 1; it != pts.end(); it++) {
		vl = splice ? sma.Push(smm.Push(it)) : it->vl;
		if (sFactor * (prev_vl - vl) > tolerance) {
			if (grow) {				// counting the beginning of the fall(growth) is done once,
									// after which it is blocked
				if (n)	sum_tm = it->tm;
				else	tm0 = it->tm;
				n++;
				grow = false;		// blocking further counting in the same direction
			}
		}
		else	grow = true;		// shange direction: unblocking counting
		prev_vl = vl;
		//cout << it->tm << '\t' << vl << LF;
	}

	_freq = --n / (sum_tm - tm0);
#ifdef _DEBUG
	cout << "\tn: " << n << LF;
#endif
}

void WaveList::FileFreq::Print() const
{
	cout << _fname << '\t' << std::setprecision(4) << (_freq / 1000) << LF;
}


bool WaveList::GetFiles(const string& dirName, const string& ext)
{
	string fileTempl = FS:: MakePath(dirName) + "*." + ext;
	WIN32_FIND_DATA ffd;
	HANDLE hFind = FindFirstFile(fileTempl.c_str(), &ffd);
	if (hFind == INVALID_HANDLE_VALUE)		return false;
	// count files to reserve files capacity
	int i = 0,  count = 0;
	do	count++;
	while (FindNextFile(hFind, &ffd));
	_files.reserve(count);
	// fill files
#ifndef _DEBUG
	cout << setw(3) << 0 << '/' << count << '\r';
#endif
	hFind = FindFirstFile(fileTempl.c_str(), &ffd);
	do {
		_files.push_back(FileFreq(ffd.cFileName));
#ifndef _DEBUG
		cout << setw(3) << ++i << '\r';
#endif
	}
	while (FindNextFile(hFind, &ffd));
	FindClose(hFind);
	return true;
}

WaveList::WaveList(const char* path)
{
	if (GetFiles(path, Ext)) {
		cout << LF;
#ifndef _DEBUG
		sort(_files.begin(), _files.end());
#endif
	}
	else cerr << "No files *." << Ext << " in given directory" << LF;
}

void WaveList::Print() const
{
	if (_files.size()) {
		cout << "file\t\tfreq, kHz\n";
		for (const auto& s : _files)	s.Print();
	}
}

/************************ end of WaveList ************************/

/************************ class FS ************************/
const char SLASH = '\\';		// standard Windows path separator
const char REAL_SLASH = '/';	// is permitted in Windows too

// Returns true if file system's object exists
//	@name: object's name
//	@st_mode: object's system mode
bool FS::IsExist(const char* name, int st_mode)
{
	struct __stat64 st;
	int res, len = strlen(name) - 1;

	if (name[len] == SLASH) {
		string sname = string(name, len);
		res = _stat64(sname.c_str(), &st);
	}
	else res = _stat64(name, &st);
	return (!res && st.st_mode & st_mode);
}

// Gets size of file or -1 if file doesn't exist
LLONG FS::Size(const char* fname)
{
	struct __stat64 st;
	return __stat64(fname, &st) == -1 ? -1 : st.st_size;
}

// Returns the name ended by slash without checking
string const FS::MakePath(const string& name)
{
	if (name.find(REAL_SLASH) != string::npos) {
		string tmp(name + SLASH);
		replace(tmp.begin(), tmp.end(), REAL_SLASH, SLASH);
		return tmp;
	}
	return name[name.length() - 1] == SLASH ? name : name + SLASH;
}

/************************ end of FS ************************/

void Timer::Stop()
{
	if (_enable) {
		long elaps = long(chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - _begin).count());
		cout << setw(2) << setfill('0') << elaps / 60 << ':' << elaps % 60 << " mm:ss\n";
	}
}
