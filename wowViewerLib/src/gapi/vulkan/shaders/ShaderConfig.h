//
// Created by Deamon on 7/3/2023.
//

#ifndef AWEBWOWVIEWERCPP_SHADERCONFIG_H
#define AWEBWOWVIEWERCPP_SHADERCONFIG_H

#include <unordered_map>
#include "../context/vulkan_context.h"

//Per DescSet -> per binding point
struct DescTypeConfig {
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    bool isBindless = false;
    int descriptorCount = 1;
    uint32_t stageMask = 0;

    bool operator==(const DescTypeConfig &other) const {
        return
            (type == other.type) &&
            (isBindless == other.isBindless) &&
            (stageMask == other.stageMask) &&
            (descriptorCount == other.descriptorCount);
    };
};

typedef std::unordered_map<int, DescTypeConfig> DescTypeSetBindingConfig;

typedef std::unordered_map<int, DescTypeSetBindingConfig> DescTypeOverride;

struct ShaderConfig {
    std::string vertexShaderFolder;
    std::string fragmentShaderFolder;
    DescTypeOverride typeOverrides;
};

#endif //AWEBWOWVIEWERCPP_SHADERCONFIG_H
