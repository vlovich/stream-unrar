#include "OS.h"

#include <sys/resource.h>
#include <unistd.h>

namespace OS {

void setPriority(ProcessPriority priority) {
	int prio;
	switch (priority) {
		case LOWEST_PRIORITY:
			prio = 10;
			break;
		case NORMAL_PRIORITY:
			prio = 0;
			break;
		case HIGHEST_PRIORITY:
			prio = -10;
			break;
	}
	::setpriority(PRIO_PROCESS, getpid(), prio);
}

}
