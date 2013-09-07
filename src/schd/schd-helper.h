#ifndef _IOCORE_SCHEDULER_H_
#define _IOCORE_SCHEDULER_H_

#include <inttypes.h>

#include <pthread.h>
#include <boost/shared_ptr.hpp>
#include <signal.h>

#include "logger.h"
#include "inlist.hpp"

namespace dh_core {

//..................................................................................... SysConf ....

class SysConf
{
public:

	static uint32_t NumCores()
	{
		uint32_t numCores = sysconf(_SC_NPROCESSORS_ONLN);
		ASSERT(numCores >= 1);

		return numCores;
	}
};

//..................................................................................... RRCpuId ....

class RRCpuId : public Singleton<RRCpuId>
{
public:

	friend class Singleton<RRCpuId>;

	uint32_t GetId()
	{
		return nextId_++ % SysConf::NumCores();
	}

private:

	RRCpuId()
	{
		nextId_ = 0;
	}

	uint32_t nextId_;
};

}

#endif