// WARNING: You are looking for a different MutexLock.h - this one is just for the aos-mkimg tool!

#ifdef AOS_MKIMG
#pragma once

class Mutex;

class MutexLock
{
public:
    MutexLock(Mutex &){};
    MutexLock(Mutex &, bool){};

    MutexLock(MutexLock const&) = delete;
    MutexLock &operator=(MutexLock const&) = delete;
};
#endif
