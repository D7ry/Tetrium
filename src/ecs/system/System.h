#pragma once
#include "structs/SharedEngineStructs.h"

#include "ecs/component/Component.h"
#include "ecs/entity/Entity.h"

class ISystem
{
  public:
    ISystem() {}

    virtual void Init(const InitContext* initData) = 0;

    // iterate through all its nodes, and perform update logic on the nodes'
    // data
    virtual void Tick(const TickContext* tickData) = 0;

    virtual void Cleanup() = 0;

    virtual ~ISystem() {}

    virtual void AddEntity(Entity* entity) { _entities.push_back(entity); }

    // TODO: handle entity removal
    // may need to make entity point to systems

  protected:
    std::vector<Entity*> _entities;
};
