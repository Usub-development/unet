#ifndef GLOBALS_H
#define GLOBALS_H
#include <vector>
#include <mutex>
#include <event.h>

extern std::vector<event_base*> eventBases;
extern std::mutex eventBasesMutex;

#endif // GLOBALS_H
