#ifndef OS_H_
#define OS_H_

namespace OS {
	enum ProcessPriority {
		LOWEST_PRIORITY,
		MEDIUM_PRIORITY,
		HIGHEST_PRIORITY,
	};

	void sleep(unsigned int sleepMS);
	void setPriority(ProcessPriority priority);
}

#endif

