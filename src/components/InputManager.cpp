#include "InputManager.h"

void InputManager::OnKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (!_active) {
        return;
    }
    switch (action) {
    case GLFW_PRESS:
        _heldKeys.insert(key);
        for (auto& callback : _pressCallbacks[key]) {
            callback();
        }
        break;
    case GLFW_RELEASE:
        _heldKeys.erase(key);
        for (auto& callback : _releaseCallbacks[key]) {
            callback();
        }
        break;
    default:
        break;
    }
}

void InputManager::ClearHeldKeys() { _heldKeys.clear(); }

void InputManager::Tick(float deltaTime) {
    if (!_active) {
        return;
    }
    for (auto& key : _heldKeys) {
        for (auto& callback : _holdCallbacks[key]) {
            callback();
        }
    }
}

void InputManager::RegisterCallback(int key, KeyCallbackCondition callbackCondition, std::function<void()> callback) {
    switch (callbackCondition) {
    case PRESS:
        if (_pressCallbacks.find(key) == _pressCallbacks.end()) {
            _pressCallbacks[key] = std::vector<std::function<void()>>();
        }
        _pressCallbacks[key].push_back(callback);
        break;
    case HOLD:
        if (_holdCallbacks.find(key) == _holdCallbacks.end()) {
            _holdCallbacks[key] = std::vector<std::function<void()>>();
        }
        _holdCallbacks[key].push_back(callback);
        break;
    case RELEASE:
        if (_releaseCallbacks.find(key) == _releaseCallbacks.end()) {
            _releaseCallbacks[key] = std::vector<std::function<void()>>();
        }
        _releaseCallbacks[key].push_back(callback);
        break;
    default:
        break;
    }
}

void InputManager::SetActive(bool active) { _active = active; }
