#pragma once
#include <GLFW/glfw3.h>

/**
 * @brief Manages GLFW input, dispatches key hold callbacks
 *
 */
class InputManager
{
  public:
    enum KeyCallbackCondition
    {
        PRESS,
        HOLD,
        RELEASE
    };

    /**
     * @brief key input function that should be bound to the glfw callback function.
     */
    void OnKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods);

    void ClearHeldKeys();

    /**
     * @brief Update function to be called once every tick. To dispatch the hold callback function.
     */
    void Tick(float deltaTime);

    /**
     * @brief Register a single key callback
     *
     * @param key the key to register
     * @param callbackCondition the condition of the input for the callback to dispatch
     * @param callback the callback function.
     */
    void RegisterCallback(int key, KeyCallbackCondition callbackCondition, std::function<void()> callback);

    void RegisterComboKeyCallback(int key, KeyCallbackCondition callbackCondition, std::function<void()> callback) {
        FATAL("Not implemented yet");
    }

  private:
    /**
     * @brief keys that are currently being pressed down
     */
    std::unordered_set<int> _heldKeys;

    /**
     * @brief Callback functions mapped to the keys, and the conditions to trigger them.
     *
     */
    std::unordered_map<int, std::vector<std::function<void()>>> _pressCallbacks = {};
    std::unordered_map<int, std::vector<std::function<void()>>> _holdCallbacks = {};
    std::unordered_map<int, std::vector<std::function<void()>>> _releaseCallbacks = {};

    struct ComboKeyMap
    {
        std::vector<int> _keys;
        std::function<void()> func;
    };

    std::vector<ComboKeyMap> _comboCallbacks;
};
