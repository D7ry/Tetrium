#pragma once

#include <deque>

class TaskQueue
{
  public:
    // push a task that would be executed at a given time.
    void Push(uint64_t time, std::function<void()> task);

    // tick the task queue, any task whose time
    // is less than TIME gets executed
    void Tick(uint64_t time);

  private:
    // queue of all tasks in increasing time order
    // order is maintained internally
    std::deque<std::pair<uint64_t, std::function<void()>>> _queue;
};
