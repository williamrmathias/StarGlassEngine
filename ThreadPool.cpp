// sge
#include "ThreadPool.h"

// Worker thread loop - spin until there is queued work
static int workerThread(void* poolPtr)
{
    ThreadPool* pool = reinterpret_cast<ThreadPool*>(poolPtr);

    while (true)
    {
        SDL_LockMutex(pool->queueMutex);

        // spin until there's a task or it's time to shutdown
        while (pool->taskCount == 0 && !pool->shutdownSignal)
            SDL_CondWait(pool->condition, pool->queueMutex);

        // if signaled to exit and queue is empty, terminate
        if (pool->shutdownSignal && pool->taskCount == 0)
        {
            SDL_UnlockMutex(pool->queueMutex);
            break;
        }

        // Pop task and execute
        Task task = pool->taskQueue[pool->queueHead];
        pool->queueHead = (pool->queueHead + 1) % kMaxTasks;
        pool->taskCount--;

        SDL_UnlockMutex(pool->queueMutex);

        if (task.function)
            task.function(task.data);

        SDL_LockMutex(pool->queueMutex);
        pool->pendingTaskCount--;

        // signal tasks done
        if (pool->pendingTaskCount == 0)
            SDL_CondBroadcast(pool->taskDoneCondition);

        SDL_UnlockMutex(pool->queueMutex);
    }

    return 0;
}

ThreadPool::ThreadPool()
{
    queueMutex = SDL_CreateMutex();
    condition = SDL_CreateCond();
    taskDoneCondition = SDL_CreateCond();

    for (uint32_t thread = 0; thread < kMaxThreads; ++thread)
        threads[thread] = SDL_CreateThread(workerThread, "Worker", reinterpret_cast<void*>(this));
}

void ThreadPool::submitTask(Task task)
{
    SDL_LockMutex(queueMutex);

    if (taskCount >= kMaxTasks)
    {
        SDL_LogError(0, "ThreadPool: Added Too Many Tasks");
        SDL_assert(!"ThreadPool: Added Too Many Tasks");
        return;
    }

    taskQueue[queueTail] = task;
    queueTail = (queueTail + 1) % kMaxTasks;
    taskCount++;
    pendingTaskCount++;

    // wake up a worker thread
    SDL_CondSignal(condition);
    SDL_UnlockMutex(queueMutex);
}

void ThreadPool::waitOnTasks()
{
    SDL_LockMutex(queueMutex);

    // sleep until taskDone is signaled
    while (pendingTaskCount > 0)
        SDL_CondWait(taskDoneCondition, queueMutex);

    SDL_UnlockMutex(queueMutex);
}

ThreadPool::~ThreadPool()
{
    SDL_LockMutex(queueMutex);
    shutdownSignal = 1;

    // wake up all workers so they can shutdown
    SDL_CondBroadcast(condition);
    SDL_UnlockMutex(queueMutex);

    for (uint32_t threadIdx = 0; threadIdx < kMaxThreads; ++threadIdx)
        SDL_WaitThread(threads[threadIdx], NULL);

    SDL_DestroyMutex(queueMutex);
    SDL_DestroyCond(condition);
    SDL_DestroyCond(taskDoneCondition);
}