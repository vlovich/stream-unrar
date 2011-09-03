#include "FilePoller.h"

#include "OS.h"

void FilePoller::waitUntilExists() const
{
	while (!exists())
		OS::sleep(500);
}

void FilePoller::waitUntilWritten() const
{
	off_t oldFlen = -1;
	off_t flen;

	waitUntilExists();

	while (oldFlen != (flen = fileSize())) {
		oldFlen = flen;
		OS::sleep(500);
	}
}

