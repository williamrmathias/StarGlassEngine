#pragma once

// stl
#include <cstdint>

// sdl
// SDL
#include <SDL2/SDL.h>

/*
* Simple thread pool for processing image load / decode tasks
* Implemented as a locked, fix sized ring buffer
*/
constexpr uint32_t kMaxThreads = 16;
constexpr uint32_t kMaxTasks = 512;

using TaskFunc = void (*)(void* data);

struct Task
{
    TaskFunc function;
    void* data;
};

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void submitTask(Task task);
    void waitOnTasks();

    SDL_Thread* threads[kMaxThreads];
    Task taskQueue[kMaxTasks] = {};

    uint32_t queueHead = 0;
    uint32_t queueTail = 0;
    uint32_t taskCount = 0;
    uint32_t pendingTaskCount = 0; // includes tasks currently running

    SDL_mutex* queueMutex;
    SDL_cond* condition;
    SDL_cond* taskDoneCondition;

    uint32_t shutdownSignal = 0;
};