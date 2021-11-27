#pragma once
#include <Windows.h>

class ThreadMutex {
private:
	CRITICAL_SECTION m_mutex;
public:
	ThreadMutex();
	~ThreadMutex();
	void Lock();
	bool TryLock();
	void Unlock();
};