//
// Created by Deamon on 11.12.22.
//

#ifndef AWEBWOWVIEWERCPP_FRAMEINPUTPARAMS_H
#define AWEBWOWVIEWERCPP_FRAMEINPUTPARAMS_H

#include <array>
#include "../../include/iostuff.h"

struct ViewPortDimensions{
    std::array<int32_t, 2> mins;
    std::array<uint32_t, 2> maxs;
};

template<typename PlanParameters>
struct FrameInputParams {
    std::shared_ptr<PlanParameters> frameParameters;

    //Time advance
    animTime_t delta = 0;
};

#endif //AWEBWOWVIEWERCPP_FRAMEINPUTPARAMS_H
