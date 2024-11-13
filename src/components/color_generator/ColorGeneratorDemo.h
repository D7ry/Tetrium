#pragma once
#include "ColorGenerator.h"

class ColorGeneratorDemo : ColorGenerator
{

    virtual PlateColor NewColor() override
    {
        return PlateColor{
            .shape = TetraColor{.RGB = {255, 255, 255}, .OCV = {255, 255, 255}},
            .background = TetraColor{.RGB = {255, 255, 255}, .OCV = {255, 255, 255}}
        };
    }

    virtual PlateColor GetColor(ColorTestResult previousResult) override
    {
        PlateColor successReturn = PlateColor{
            .shape = TetraColor{.RGB = {255, 255, 255}, .OCV = {255, 255, 255}},
            .background = TetraColor{.RGB = {255, 255, 255}, .OCV = {255, 255, 255}}
        };

        PlateColor failReturn = PlateColor{
            .shape = TetraColor{.RGB = {255, 255, 255}, .OCV = {255, 255, 255}},
            .background = TetraColor{.RGB = {255, 255, 255}, .OCV = {255, 255, 255}}
        };

        switch (previousResult) {
            case ColorTestResult::kSuccess:
                return successReturn;
            case ColorTestResult::kFailure:
                return failReturn;
        }
    }
};
