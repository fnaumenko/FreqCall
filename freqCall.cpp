/************************************************************************************
freqCall - frequency recognition - recognition of frequencies recorded in files.
This is test for Pico Technology

Fedor Naumenko (fedor.naumenko@gmail.com), 26.07.2020

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

int main(int argc, char* argv[]) {
	bool setTimer = false;
	char* par = NULL;
	// *** parsing parameters ***
	for (int i=1; i<argc; i++) {
		char* opt = argv[i];
		if (opt[0] == '-') {
			if (opt[1] == 't')		setTimer = true;
			else if (opt[1] == 'l')	cout.imbue(locale("English"));
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

// Extract values from line
bool ParseLine(const string& line, float& tm, float& vl)
{
	istringstream iss(line);
	if (!(iss >> tm))	return true;
	iss.get();						// skip comma
	if (!(iss >> vl))	return true;
	return false;
}

// Initializes the created instance
//	@fname: file name
WaveList::File_freq::File_freq(const string& fname)	: _fname(fname), _freq(0)
{
	fstream fs(_fname, ios::in);
	if (!fs.is_open())	return;

	const int initCnt = 3;					// count of first pre-read lines
	string line;							// current read line
	int i = 0, k = initCnt;					// full period counter, line counter
	float	tm, vl, sum_tm = 0,				// current time (sec), value (volts), sum of all periods
			prev_tm = 0, prev_vl = FLT_MIN,	// previous time, value
			min_vl = FLT_MAX,		// current min value; some problrms with compilation using std::numeric_limits
			max_vl = -FLT_MAX,		// current max value
			diff = 0;		// difference between lower and upper noise levels
	int	climbs = 0;			// continuous ascent counter: incremented if each next value is more than prev one
	bool get_diff = true;	// if true than accumulate noise difference
	SMA sma(10);			// splicer
	
	float vls[initCnt];

	getline(fs, line);					// skip title
	for (int x = 0; x < initCnt; x++) {	// pre-read lines
		if(!getline(fs, line) && ParseLine(line, tm, vls[x]))		return;
		sma.Push(vls[x]);				// fill splicer's buffer
	}
	bool flat = true;
	diff = 0;
	for (int x = 1; x < initCnt; x++) {
		if (vls[x] != vls[x - 1]) {
			flat = false;
			float diff0 = vls[x] - vls[x - 1];
			if (diff0 > 0)
				climbs++;
			if (abs(diff0) > diff)	diff = abs(diff0);
		}
	}
	//diff *= 5;
	prev_vl = vls[initCnt-1];

	while (getline(fs, line)) {
		if (ParseLine(line, tm, vl))	return;

		//if (!flat) {
		//	float vl1 = sma.Push(vl);
		//	if (vl1 != FLT_MIN)
		//		vl = vl1;
		//}
		k++;
		if (vl >= prev_vl) {
			if (max_vl < vl)
				max_vl = vl;
			climbs++;
		}
		else {
			if (min_vl > vl)	min_vl = vl;
			if (flat || climbs > 2 || (diff && (prev_vl - vl) > diff)) {			// count from the second round
				sum_tm += tm - prev_tm;
				i++;
				get_diff = false;
				prev_tm = tm;
				climbs = 0;
			}
		}
		prev_vl = vl;
		if (get_diff && max_vl != -FLT_MAX && min_vl != FLT_MAX)
			diff = max(diff, 5 * abs(max_vl - min_vl));
	}
	i--;
#ifdef _DEBUG
	cout << _fname << "\ti: " << i << "  max_vl: " << max_vl << "  mean: " << (sum_tm / i) << '\n';
#endif
	_freq = i / sum_tm;
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
	files.reserve(count);
	// fill files
	cout << setw(3) << 0 << '/' << count << '\r';
	hFind = FindFirstFile(fileTempl.c_str(), &ffd);
	do {
		files.push_back(File_freq(ffd.cFileName));
		cout << setw(3) << ++i << '\r';
	}
	while (FindNextFile(hFind, &ffd));
	FindClose(hFind);
	return true;
}

WaveList::WaveList(const char* path)
{
	if (GetFiles(path, ext)) {
		cout << LF;
		sort(files.begin(), files.end());
	}
	else cerr << "No files *." << ext << " in given directory" << LF;
}

void WaveList::Print() const
{
	if (files.size()) {
		cout << "file\t\tfreq, kHz\n";
		for (const auto& s : files)	s.Print();
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