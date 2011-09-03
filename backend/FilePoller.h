#ifndef FILE_POLLER_H_
#define FILE_POLLER_H_

#include <string>

class FilePoller {
public:
	FilePoller(const std::string& path);
	~FilePoller();

	void waitUntilExists() const;
	void waitUntilWritten() const;

protected:
	off_t fileSize() const;
	bool exists() const;

private:
	std::string m_pollPath;
};

#endif

