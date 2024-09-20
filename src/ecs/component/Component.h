#pragma once

// base component class
class Entity;

class IComponent
{
  public:
    virtual ~IComponent() {}

    Entity* parent;
};
