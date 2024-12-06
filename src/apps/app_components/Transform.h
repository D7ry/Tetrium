#pragma once
struct Transform
{
    glm::vec3 position = glm::vec3(0.f);
    glm::vec3 rotation = glm::vec3(0.f); // yaw pitch roll
    glm::vec3 scale = glm::vec3(1.f);    // x y z

    // get identity transform
    static Transform Identity()
    {
        Transform id;
        id.position = glm::vec3(0.f);
        id.rotation = glm::vec3(0.f);
        id.scale = glm::vec3(1.f);
        return std::move(id);
    }

    void GetModelMatrix(glm::mat4& model)
    {
        model = glm::mat4(1.f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.f, 0.f, 0.f));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.f, 1.f, 0.f));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.f, 0.f, 1.f));
        model = glm::scale(model, scale);
    }

    // get model matrix of the transform
    glm::mat4 GetModelMatrix()
    {
        glm::mat4 model;
        GetModelMatrix(model);
        return model;
    }
};
