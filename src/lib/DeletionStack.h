#pragma once

/**
 * @brief A stack of functions to be executed that handles the deletion of resources.
 * Stack is FIFO, so push cleaning functions right after resource initialization.
 *
 */
struct DeletionStack
{
    void push(std::function<void()>&& function) { _deleters.push_back(function); }

    /**
     * @brief Execute all deletion functions in the stack.
     *
     */
    void flush() {
        for (auto it = _deleters.rbegin(); it != _deleters.rend(); it++) {
            (*it)();
        }
        _deleters.clear();
    }

    ~DeletionStack() {
        if (!_deleters.empty()) {
            PANIC("Deletion stack not emptied. Please use DeletionStack::flush() to empty the stack");
        }
    }

  private:
    std::vector<std::function<void()>> _deleters;
};
