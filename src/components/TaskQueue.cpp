#include "TaskQueue.h"

void TaskQueue::Tick(uint64_t time)
{
    bool popping = !_queue.empty();
    while (popping) {
        if (_queue.front().first <= time) {
            auto fun = _queue.front().second;
            fun();
            _queue.pop_front();
        } else {
            popping = false;
        }
        popping &= !_queue.empty();
    }
}

void TaskQueue::Push(uint64_t time, std::function<void()> task)
{
    int left = 0;
    int right = _queue.size();

    // Binary search to find the correct insertion position
    while (left < right) {
        int mid = left + (right - left) / 2;
        if (_queue[mid].first < time) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    // Insert the new task at the correct position
    _queue.insert(_queue.begin() + left, std::make_pair(time, task));
}

