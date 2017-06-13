/*
Stream Unrar - Start multi-part rar extraction as soon as the first part is available and wait
silently until more are available.

Copyright (C)  2007, 2008, 2011  Vitali Lovich

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Stream unrar homepage : http://www.gnu.org/software/liquidwar6/
*/

/* TODO: Cleanup code and comments - this is just a proof-of-concept so far. */
/* TODO: Refactor code properly into headers & source */
/* TODO: Refactor OS-dependant code into seperate files */

#include <sys/stat.h>

#if !defined(_WIN32) || defined(__CYGWIN)

/* *NIX */

#define _UNIX
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <dirent.h>

#ifdef __linux
#define LINUX
#endif

#if defined(LINUX) || defined(_UNIX)

/* LINUX */

#include <sys/resource.h>
#define LOWEST_PRIORITY -10
#define NORMAL_PRIORITY 0
#define HIGH_PRIORITY 10

#endif /* LINUX */

#else

/* WINDOWS */

#define WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <intrin.h>
#define LOWEST_PRIORITY THREAD_PRIORITY_IDLE
#define NORMAL_PRIORITY THREAD_PRIORITY_NORMAL
#define HIGH_PRIORITY THREAD_PRIORITY_HIGHEST

#endif

#include <stdio.h>
#include <string>
#include <algorithm>
#include <locale>
#include <vector>
#include <cassert>

//#include "unrar.h"
#include "rar.hpp"

#ifdef WINDOWS
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

static const std::string add_flag = "-a";
static const std::string dir_flag = "-d";
static const std::string monitor_flag = "-m";
static const std::string pass_flag = "-p";
static const std::string recurse_flag = "-r";
static const std::string extract_flag = "-e";
static const std::string abort_monitor = "stop.monitoring";

static std::vector<std::string> extracted_files;

struct fileInfo
{
	bool isDir;
	bool isFile;
};

enum _priority {low, normal, high};

void setPriority(_priority level)
{
	int priority;
	switch(level)
	{
	case low:
		priority = LOWEST_PRIORITY;
		break;
	case normal:
		priority = NORMAL_PRIORITY;
		break;
	case high:
		priority = HIGH_PRIORITY;
		break;
	}
#ifdef WINDOWS
	SetThreadPriority(GetCurrentThread(), priority);
#else
	setpriority(PRIO_PROCESS, getpid(), priority);
#endif
}

void thread_sleep(int amountInSeconds)
{
#ifdef WINDOWS
	Sleep(amountInSeconds * 1000);
#else
	sleep(amountInSeconds);
#endif
}

bool getPathInfo(const char * name, fileInfo & info)
{
	// we don't handle symbolic links as destinations
	if (name == NULL)
		return false;

	int result;
#ifdef WINDOWS
	struct __stat64 buf;
	result = _stat64(name, &buf);
#elif __LP64__
	struct stat buf;
	result = stat(name, &buf);
#else
	struct stat64 buf;
	result = stat64(name, &buf);
#endif

	info.isDir = (buf.st_mode & S_IFDIR) != 0;
	info.isFile = (buf.st_mode & S_IFREG) != 0;

	return result == 0 ? true : false;
}

bool pathExists(const std::string & name)
{
	fileInfo info;
	return getPathInfo(name.c_str(), info);
}

bool dirExists(const std::string & name)
{
	fileInfo info;
	return getPathInfo(name.c_str(), info) && info.isDir;
}

bool fileExists(const char * name)
{
	fileInfo info;
	return getPathInfo(name, info) && info.isFile;
}

bool fileExists(const std::string & name)
{
	fileInfo info;
	return getPathInfo(name.c_str(), info) && info.isFile;
}

bool createDir(const char * name)
{
	fileInfo info;

	if (getPathInfo(name, info))
		return info.isDir;

#ifdef WINDOWS
	return 0 == _mkdir(name);
#else
	return 0 == mkdir(name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
}

bool createDir(const std::string & name)
{
	return createDir(name.c_str());
}

struct MultipartInfo
{
#ifdef _MSC_VER
	unsigned __int64
#else
	uint64_t
#endif
		bytes_processed;
};


int CALLBACK missing_file(UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2)
{
	//fprintf(stderr, "Callback %u\n", msg);
	switch(msg)
	{
		case UCM_CHANGEVOLUME:
			if (P2 == RAR_VOL_ASK)
			{
				fprintf(stderr, "Waiting for %s\n", (char*)P1);
				while (!fileExists((char*)P1))
				{
					thread_sleep(4);
				}

				thread_sleep(3);

				fprintf(stderr, "%s found\n", (char*)P1);
			}
			break;
		case UCM_PROCESSDATA:
			if (UserData)
				((MultipartInfo *)UserData)->bytes_processed += P2;
			break;
		default:
			break;
	}
	return 1;
}

int parseOpts(int argc, char ** argv, std::vector<std::string> & files_to_unpack, std::vector<std::string> & dest_dir, std::vector<std::string> & passwords, std::vector<std::string> & monitor_dirs, bool & recurse, std::string & extract_dir)
{
	// The options could come in any order - however, the password for an archive
	// must come after the archive is specified and before the next archive
	monitor_dirs.clear();
	dest_dir.clear();
	files_to_unpack.clear();
	passwords.clear();
	extract_dir.clear();
	recurse = false;

	int archive_count = 0;

	for(int i = 0; i < argc; ++i)
	{
		//fprintf(stderr, "processing argument %s\n", argv[i]);
		if (argv[i] == add_flag)
		{
			if (++i == argc)
				// out of arguments
				return -1;
			files_to_unpack.push_back(argv[i]);
			++archive_count;
			continue;
		}
		if (argv[i] == dir_flag)
		{
			if (++i == argc)
				// out of arguments
				return -1;
			dest_dir.push_back(argv[i]);
			continue;
		}
		if (argv[i] == pass_flag)
		{
			if (++i == argc)
				// out of arguments
				return -1;
			
			if (passwords.size() >= archive_count)
				// password specified out of order
				return -1;

			while(passwords.size() < archive_count)
				// previous archives didn't have a password specified
				passwords.push_back("");

			passwords.push_back(argv[i]);
			continue;
		}
		if (argv[i] == monitor_flag)
		{
			if (++i == argc)
				// out of arguments
				return -1;

			monitor_dirs.push_back(argv[i]);
			continue;
		}
		if (argv[i] == extract_flag)
		{
			if (++i == argc)
				// out of arguments
				return -1;

			if (!extract_dir.empty())
			{
				fprintf(stderr, "Extraction directory for monitored directories is specified too many times\n");
				return -4;
			}

			extract_dir = argv[i];

			continue;
		}

		recurse = (argv[i] == recurse_flag);
	}

	if (dest_dir.size() == 0 && 0 == files_to_unpack.size() && 0 == monitor_dirs.size())
	{
		// no action to take
		return -3;
	}

	if (dest_dir.size() != files_to_unpack.size())
	{
		return -2;
	}

	// assume empty passwords for remaining archives
	while(passwords.size() < archive_count)
		// previous archives didn't have a password specified
		passwords.push_back("");

	return 0;
}

#ifdef WINDOWS
DWORD start_time;
#else
time_t start_time;
#endif

const int five_seconds = 5000;

void start_timer()
{
#ifdef WINDOWS
	start_time = GetTickCount();
#else
	time(&start_time);
#endif
}

unsigned long calc_elapsed()
{
#ifdef WINDOWS
	return (GetTickCount() - start_time);
#else
	// difftime returns the number of seconds
	unsigned long result = (unsigned long)difftime(time(NULL), start_time);
	return result * 1000;
#endif
}

bool elapsed(unsigned int time_millis)
{
	return calc_elapsed() >= time_millis;
}

void toLower(std::string & s)
{
	std::transform(s.begin(), s.end(), s.begin(), tolower);
}

bool contains(const std::vector<std::string> & stringList, const std::string & string, bool caseSensitive)
{
	std::string toSearch = string;
	std::string list_value;

	for(std::vector<std::string>::const_iterator i = stringList.begin(); i != stringList.end(); ++i)
	{
		list_value = *i;
		if (!caseSensitive)
		{
			toLower(list_value);
			toLower(toSearch);
		}

		if (list_value == toSearch)
			return true;
	}
	return false;
}

void extract(const std::string & archive_name, const std::string & dest_dir, const std::string & password)
{
	bool already_extracted;
#ifdef WINDOWS
	already_extracted = contains(extracted_files, archive_name, false);
#else
	already_extracted = contains(extracted_files, archive_name, true);
#endif

	if (already_extracted)
		return;

	fprintf(stdout, "Extracting %s to %s\n", archive_name.c_str(), dest_dir.c_str());

	HANDLE archive;
	struct RAROpenArchiveData data;
	struct RARHeaderData header;

	if (!fileExists(archive_name))
		return;

	if (!createDir(dest_dir))
		return;

	data.ArcName = (char *)archive_name.c_str();
	data.OpenMode = RAR_OM_EXTRACT;
	data.CmtBuf = NULL;
	data.CmtBufSize = 0;


	//fprintf(stdout, "Opening %s\n", argv[1]);
	archive = RAROpenArchive(&data);

	if (!archive || data.OpenResult != 0)
	{
		fprintf(stderr, "Couldn't open %s\n", archive_name.c_str());
		return;
	}

	header.CmtBuf = NULL;

	//fprintf(stdout, "Clearing password\n");
	RARSetPassword(archive, (char*)password.c_str());

	MultipartInfo info;
	//fprintf(stdout, "Setting callback\n");
	RARSetCallback(archive, &missing_file, (ssize_t)&info);

	//fprintf(stdout, "Starting to process file\n");

	// the complete path to the file to be extracted
	std::string dest_path;

	start_timer();
	while (!RARReadHeader(archive, &header))
	{
		dest_path = dest_dir + PATH_SEP + header.FileName;
		if (pathExists(dest_path))
		{
			fprintf(stderr, "Skpping %s\n",  header.FileName);
			continue;
		}
		if (RARProcessFile(archive, RAR_EXTRACT, (char *)dest_dir.c_str(), NULL))
		{
			/*if (!pathExists(dest_path))
			{
				fprintf(stderr, "Couldn't create %s\n",  dest_path.c_str());
				break;
			}*/

			if (elapsed(five_seconds))
			{
				info.bytes_processed = 0;
				start_timer();
			}
		}
		else
		{
			break;
		}

		// (bytes / (1024 * 1024) = MBytes
		// ms / 1000 = s
		// (bytes / (1024 * 1024) / (ms / 1000) = 0.00095367431640625 * bytes/ms
		//fprintf(stdout, "%d MB/s", (int)(0.00095367431640625 * (info.bytes_processed / calc_elapsed())));
	}
	//fprintf(stdout, "Closing file\n");
	RARCloseArchive(archive);

	extracted_files.push_back(archive_name);
}

void listFiles(const std::string & dir, const bool recurse, std::vector<std::string> & files, bool & abort)
{
	std::vector<std::string> childDirs;
	std::string child_name;

#ifdef WINDOWS
	WIN32_FIND_DATA fileData;
	std::string child_search = (dir + PATH_SEP) + "*";
	HANDLE fileHandle = FindFirstFile(child_search.c_str(), &fileData);

	// empty directory
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	// process the next child in the driectory
	while (FindNextFile(fileHandle, &fileData) != 0) 
	{
		child_name = fileData.cFileName;
#else
	DIR * directory = opendir(dir.c_str());
	struct dirent entry;
	struct dirent * next_entry;
	if (NULL == directory)
	{
		return;
	}

	while (0 == readdir_r(directory, &entry, &next_entry) && next_entry != NULL)
	{
		child_name = entry.d_name;
#endif

		abort = (abort_monitor == child_name);

		// if the child represents the directory itself, or its parent, skip
		if (child_name == "." || child_name == "..")
			continue;

		// build the full path
		std::string child = dir + PATH_SEP + child_name;

		// deal appropriately with the file depending on whether it is a directory or file
/*#ifdef WINDOWS
		if ((fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
#else
#error Not implemented
#endif
*/
		if (dirExists(child))
		{
			// if not doing a recursive search, no point in even adding to the list
			if (recurse)
				continue;

			childDirs.push_back(child);
		}
		else
		{
			files.push_back(child);
		}
	}

#ifdef WINDOWS
	FindClose(fileHandle);
#else
	closedir(directory);
#endif

	if (recurse)
	{
		for(std::vector<std::string>::const_iterator cDir = childDirs.begin(); cDir != childDirs.end(); ++cDir)
			listFiles(*cDir, recurse, files, abort);
	}
}

// not passed-by-ref on purpose - need a copy here
bool hasRarSuffix(std::string s)
{
	// file must be at least a.rar long
	if (s.length() < 5)
		return false;
	
	toLower(s);

	// index s.length() - 1 points to r
	// index s.length() - 4 points to .
	return (s.substr(s.length() - 4) == ".rar");
}

bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

bool hasR01Suffix(std::string s)
{
	// .rXX
	s = s.substr(s.length() - 4);

	return s == ".r01";
}

bool hasPartXXNaming(std::string s)
{
	// .part01.rar
	s = s.substr(s.length() - 11);
	toLower(s);

	// note : already established that has suffix .rar 
	return	s[0] == '.' &&
			s[1] == 'p' &&
			s[2] == 'a' &&
			s[3] == 'r' &&
			s[4] == 't' &&
			isDigit(s[5]) &&
			isDigit(s[6]);
}

bool isPart01(std::string s)
{
	// .part01.rar
	s = s.substr(s.length() - 11);
	toLower(s);

	// note : already established that has suffix .rar 
	return s == ".part01.rar";
}

bool has001Suffix(std::string s)
{
	// file name ends with 001
	if (s.length() < 5)
		return false;
	
	// unnecessary since comparing against non-alphabetic chars
	// toLower(s);

	// index s.length() - 1 points to 1
	// index s.length() - 4 points to .
	return (s.substr(s.length() - 4) == ".001");
}

// finds the first part of a multi-part archive (or the whole archive itself if there's only 1
void findRars(const std::vector<std::string> & files, std::vector<std::string> & rars)
{
	for (std::vector<std::string>::const_iterator f = files.begin(); f != files.end(); ++f)
	{
		std::string file = *f;

		bool firstPart = (hasRarSuffix(file) && (!hasPartXXNaming(file) || isPart01(file))) || has001Suffix(file);

		if (firstPart)
		{
			rars.push_back(file);
		}
	}
}

bool process(const std::string & dir, bool recurse, const std::string & dest_dir)
{
	std::vector<std::string> files;
	std::vector<std::string> rars;
	bool abort = false;

	listFiles(dir, recurse, files, abort);
	findRars(files, rars);
	for (std::vector<std::string>::const_iterator i = rars.begin(); i != rars.end(); ++i)
	{
		setPriority(high);
		// no password support for monitoring
		extract(*i, dest_dir, std::string(""));
		setPriority(low);
	}

	return abort;
}

int main(int argc, char ** argv)
{
/*	OpenArchive roa;
	SetCallback rsc;
	ProcessFile rpf;
	CloseArchive rca;*/

	std::vector<std::string> monitor_dirs;
	std::string extract_dir;
	std::vector<std::string> files_to_unpack;
	std::vector<std::string> dest_dirs;
	std::vector<std::string> passwords;
	bool recurseMonitor;

	if (parseOpts(argc, argv, files_to_unpack, dest_dirs, passwords, monitor_dirs, recurseMonitor, extract_dir))
	{
		fprintf(stderr, "Usage: stream_unrar [%s <rar file> %s <dest_dir> [%s <password>]] [%s <monitor_dir> %s <extract_dir> [%s]]\n", add_flag.c_str(), dir_flag.c_str(), pass_flag.c_str(), monitor_flag.c_str(), extract_flag.c_str(), recurse_flag.c_str());
		fprintf(stderr, "Multiple archives or monitor directories can be specified.\n");
		fprintf(stderr, "Archives are processed first, then monitoring begins\n");
		fprintf(stderr, "Monitoring is stopped when a file %s is created in one of the directories\n", abort_monitor.c_str());
		return -1;
	}

	assert(files_to_unpack.size() == dest_dirs.size());
	assert(files_to_unpack.size() == passwords.size());

	for(std::vector<std::string>::iterator
		file = files_to_unpack.begin(), dir = dest_dirs.begin(), pass = passwords.begin(); 
		// we're guaranteed that all 3 iterators are the same size by parseOpts
		file != files_to_unpack.end(); ++file, ++dir, ++pass)
	{
		extract(*file, *dir, *pass);		
	}
	
	if (monitor_dirs.size() == 0)
		return 0;

	setPriority(low);

	fprintf(stdout, "Beginning to monitor watch directories\n");
	bool stopMonitoring = false;
	bool valid_monitor_dir = true;
	while (!stopMonitoring && valid_monitor_dir)
	{
		valid_monitor_dir = false;
		for (std::vector<std::string>::iterator dir = monitor_dirs.begin();
			dir != monitor_dirs.end(); ++ dir)
		{
			if (!dirExists(*dir))
				continue;
			valid_monitor_dir = true;
			if (process(*dir, recurseMonitor, extract_dir))
				return 0;
		}
		thread_sleep(1);
	}
	return -1;
}
