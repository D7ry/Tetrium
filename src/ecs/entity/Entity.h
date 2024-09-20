#pragma once
#include <typeindex>

#include "ecs/component/Component.h"

// an entity
// entity is the basic unit of object
// an entity can contain multiple components
// an entity can only contain one component per component type.
// call AddComponent<T> to add a new component
// components are then updated by the system
class Entity
{
  public:
    Entity(const std::string& name) { this->_name = name; }

    template <typename T, typename... Args>
    void CreateComponent(Args&&... args)
    {
        _components.emplace(std::type_index(typeid(T)), new T(std::forward<Args>(args)...));
    }

    template <typename T>
    void AddComponent(T* component)
    {
        _components[std::type_index(typeid(T))] = component;
        component->parent = this; // back link component to the entity
    }

    // get a component of type `T` of the entity.
    // returns `nullptr` if the entity does not have such component
    template <typename T>
    T* GetComponent()
    {
        auto it = _components.find(std::type_index(typeid(T)));
        if (it == _components.end()) {
            return nullptr;
        }
        return static_cast<T*>(it->second);
    }

    const char* GetName() { return this->_name.c_str(); }

  private:
    std::unordered_map<std::type_index, IComponent*> _components;
    std::string _name;
};
