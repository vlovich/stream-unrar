#include "FilePoller.h"

#include <sys/stat.h>
#include <cassert>
#include <errno.h>

off_t FilePoller::fileSize() const
{
	struct stat info;
	if (0 != stat(m_pollPath.c_str(), &info))
		return -1;
	return info.st_size;
}

bool FilePoller::exists() const
{
	struct stat info;
	if (0 == stat(m_pollPath.c_str(), &info))
		return true;
	if (ENOENT == errno)
		return false;
	assert(false);
	return false; 
}
