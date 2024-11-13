#pragma once

// Generates color specifications for psychophysics tests
class ColorGenerator
{
  public:
    enum class ColorTestResult {
        kSuccess,
        kFailure
    };
    struct TetraColor
    {

        glm::vec3 RGB;
        glm::vec3 OCV;
    };

    struct PlateColor
    {
        TetraColor shape;
        TetraColor background;
    };

    virtual PlateColor NewColor() = 0;
    virtual PlateColor GetColor(ColorTestResult previousResult) = 0;

};
