#if 0
#pragma once

// base component class
class Entity;

class IComponent
{
  public:
    virtual ~IComponent() {}

    Entity* parent;
};
#endif
