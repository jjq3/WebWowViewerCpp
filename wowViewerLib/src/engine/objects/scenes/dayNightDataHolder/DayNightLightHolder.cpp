//
// Created by Deamon on 7/5/2024.
//

#include "DayNightLightHolder.h"
#include "../../../../include/database/dbStructs.h"
#include "../../../algorithms/mathHelper.h"
#include "LightParamCalculate.h"


DayNightLightHolder::DayNightLightHolder(const HApiContainer &api, int mapId) : m_api(api), m_mapId(mapId) {

    MapRecord mapRecord;
    if (m_api) {
        api->databaseHandler->getMapById(mapId, mapRecord);
        m_mapFlag2_0x2 = (mapRecord.flags2 & 0x2) > 0;
        m_useWeightedBlend = (mapRecord.flags0 & 0x4) > 0;
        m_mapHasFlag_0x200000 = (mapRecord.flags0 & 0x200000) > 0;
        m_mapHasFlag_0x10000 = (mapRecord.flags0 & 0x10000) > 0;
    }

    createMinFogDistances();
}

void DayNightLightHolder::loadZoneLights() {
    if (m_api && m_api->databaseHandler != nullptr) {
        m_zoneLights = loadZoneLightRecs(m_api->databaseHandler, m_mapId);
    }
}


static inline mathfu::vec4 mix(const mathfu::vec4 &a, const mathfu::vec4 &b, float alpha) {
    return (b - a) * alpha + a;
}
static inline mathfu::vec3 mix(const mathfu::vec3 &a, const mathfu::vec3 &b, float alpha) {
    return (b - a) * alpha + a;
}
static inline float mix(const float &a, const float &b, float alpha) {
    return (b - a) * alpha + a;
}

static inline mathfu::vec4 mad(const mathfu::vec4 &a, const mathfu::vec4 &b, float alpha) {
    return a + b * alpha;
}
static inline mathfu::vec3 mad(const mathfu::vec3 &a, const mathfu::vec3 &b, float alpha) {
    return a + b * alpha;
}
static inline float mad(const float &a, const float &b, float alpha) {
    return a + b * alpha;
}

template <typename T, typename S>
void mixStructOffset(T& a, T& b, S T::*member, float blendTimeCoeff) {
    a.*member = mix(a.*member, b.*member, blendTimeCoeff);
}

template <typename T, typename S>
void madStructOffset(T& a, T& b, S T::*member, float blendTimeCoeff) {
    a.*member = mad(a.*member, b.*member, blendTimeCoeff);
}

template <typename T>
void mixStructure(T &a, T &b, float blendCoeff);

template <>
void mixStructure<FogResult>(FogResult& a, FogResult& b, float blendCoeff) {
    mixStructOffset(a, b, &FogResult::FogEnd                       , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogScaler                    , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogDensity                   , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogHeight                    , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogHeightScaler              , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogHeightDensity             , blendCoeff);
    mixStructOffset(a, b, &FogResult::SunFogAngle                  , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogColor                     , blendCoeff);
    mixStructOffset(a, b, &FogResult::EndFogColor                  , blendCoeff);
    mixStructOffset(a, b, &FogResult::EndFogColorDistance          , blendCoeff);
    mixStructOffset(a, b, &FogResult::SunFogColor                  , blendCoeff);
    mixStructOffset(a, b, &FogResult::SunFogStrength               , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogHeightColor               , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogHeightCoefficients        , blendCoeff);
    mixStructOffset(a, b, &FogResult::MainFogCoefficients          , blendCoeff);
    mixStructOffset(a, b, &FogResult::HeightDensityFogCoefficients , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogZScalar                   , blendCoeff);
    mixStructOffset(a, b, &FogResult::LegacyFogScalar              , blendCoeff);
    mixStructOffset(a, b, &FogResult::MainFogStartDist             , blendCoeff);
    mixStructOffset(a, b, &FogResult::MainFogEndDist               , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogBlendAlpha                , blendCoeff);
    mixStructOffset(a, b, &FogResult::HeightEndFogColor            , blendCoeff);
    mixStructOffset(a, b, &FogResult::FogStartOffset               , blendCoeff);
    mixStructOffset(a, b, &FogResult::SunAngleBlend                , blendCoeff);
}

template <>
void mixStructure<SkyColors>(SkyColors& a, SkyColors& b, float blendCoeff) {
    mixStructOffset(a, b, &SkyColors::SkyTopColor        , blendCoeff);
    mixStructOffset(a, b, &SkyColors::SkyMiddleColor     , blendCoeff);
    mixStructOffset(a, b, &SkyColors::SkyBand1Color      , blendCoeff);
    mixStructOffset(a, b, &SkyColors::SkyBand2Color      , blendCoeff);
    mixStructOffset(a, b, &SkyColors::SkySmogColor       , blendCoeff);
    mixStructOffset(a, b, &SkyColors::SkyFogColor        , blendCoeff);
}

template <>
void mixStructure<ExteriorColors>(ExteriorColors& a, ExteriorColors& b, float blendCoeff) {
    mixStructOffset(a, b, &ExteriorColors::exteriorAmbientColor          , blendCoeff);
    mixStructOffset(a, b, &ExteriorColors::exteriorHorizontAmbientColor  , blendCoeff);
    mixStructOffset(a, b, &ExteriorColors::exteriorGroundAmbientColor    , blendCoeff);
    mixStructOffset(a, b, &ExteriorColors::exteriorDirectColor           , blendCoeff);
}
template <>
void mixStructure<LiquidColors>(LiquidColors& a, LiquidColors& b, float blendCoeff) {
    mixStructOffset(a, b, &LiquidColors::closeRiverColor  , blendCoeff);
    mixStructOffset(a, b, &LiquidColors::farRiverColor    , blendCoeff);
    mixStructOffset(a, b, &LiquidColors::closeOceanColor  , blendCoeff);
    mixStructOffset(a, b, &LiquidColors::farOceanColor    , blendCoeff);

    mixStructOffset(a, b, &LiquidColors::riverShallowAlpha , blendCoeff);
    mixStructOffset(a, b, &LiquidColors::riverDeepAlpha    , blendCoeff);
    mixStructOffset(a, b, &LiquidColors::oceanShallowAlpha , blendCoeff);
    mixStructOffset(a, b, &LiquidColors::oceanDeepAlpha    , blendCoeff);
}
template <>
void mixStructure<SkyBodyData>(SkyBodyData& a, SkyBodyData& b, float blendCoeff) {
    mixStructOffset(a, b, &SkyBodyData::celestialBodyOverride  , blendCoeff);
    mixStructOffset(a, b, &SkyBodyData::celestialBodyOverride2 , blendCoeff);
}

template <typename T>
//Multiply and Add
void madStructure(T &a, T &b, float blendCoeff);


template <>
void madStructure<FogResult>(FogResult& a, FogResult& b, float blendCoeff) {
    madStructOffset(a, b, &FogResult::FogEnd                       , blendCoeff);
    madStructOffset(a, b, &FogResult::FogScaler                    , blendCoeff);
    madStructOffset(a, b, &FogResult::FogDensity                   , blendCoeff);
    madStructOffset(a, b, &FogResult::FogHeight                    , blendCoeff);
    madStructOffset(a, b, &FogResult::FogHeightScaler              , blendCoeff);
    madStructOffset(a, b, &FogResult::FogHeightDensity             , blendCoeff);
    madStructOffset(a, b, &FogResult::SunFogAngle                  , blendCoeff);
    madStructOffset(a, b, &FogResult::FogColor                     , blendCoeff);
    madStructOffset(a, b, &FogResult::EndFogColor                  , blendCoeff);
    madStructOffset(a, b, &FogResult::EndFogColorDistance          , blendCoeff);
    madStructOffset(a, b, &FogResult::SunFogColor                  , blendCoeff);
    madStructOffset(a, b, &FogResult::SunFogStrength               , blendCoeff);
    madStructOffset(a, b, &FogResult::FogHeightColor               , blendCoeff);
    madStructOffset(a, b, &FogResult::FogHeightCoefficients        , blendCoeff);
    madStructOffset(a, b, &FogResult::MainFogCoefficients          , blendCoeff);
    madStructOffset(a, b, &FogResult::HeightDensityFogCoefficients , blendCoeff);
    madStructOffset(a, b, &FogResult::FogZScalar                   , blendCoeff);
    madStructOffset(a, b, &FogResult::LegacyFogScalar              , blendCoeff);
    madStructOffset(a, b, &FogResult::MainFogStartDist             , blendCoeff);
    madStructOffset(a, b, &FogResult::MainFogEndDist               , blendCoeff);
    madStructOffset(a, b, &FogResult::FogBlendAlpha                , blendCoeff);
    madStructOffset(a, b, &FogResult::HeightEndFogColor            , blendCoeff);
    madStructOffset(a, b, &FogResult::FogStartOffset               , blendCoeff);
    madStructOffset(a, b, &FogResult::SunAngleBlend                , blendCoeff);
}

template <>
void madStructure<SkyColors>(SkyColors& a, SkyColors& b, float blendCoeff) {
    madStructOffset(a, b, &SkyColors::SkyTopColor        , blendCoeff);
    madStructOffset(a, b, &SkyColors::SkyMiddleColor     , blendCoeff);
    madStructOffset(a, b, &SkyColors::SkyBand1Color      , blendCoeff);
    madStructOffset(a, b, &SkyColors::SkyBand2Color      , blendCoeff);
    madStructOffset(a, b, &SkyColors::SkySmogColor       , blendCoeff);
    madStructOffset(a, b, &SkyColors::SkyFogColor        , blendCoeff);
}

template <>
void madStructure<ExteriorColors>(ExteriorColors& a, ExteriorColors& b, float blendCoeff) {
    madStructOffset(a, b, &ExteriorColors::exteriorAmbientColor          , blendCoeff);
    madStructOffset(a, b, &ExteriorColors::exteriorHorizontAmbientColor  , blendCoeff);
    madStructOffset(a, b, &ExteriorColors::exteriorGroundAmbientColor    , blendCoeff);
    madStructOffset(a, b, &ExteriorColors::exteriorDirectColor           , blendCoeff);
}
template <>
void madStructure<LiquidColors>(LiquidColors& a, LiquidColors& b, float blendCoeff) {
    madStructOffset(a, b, &LiquidColors::closeRiverColor  , blendCoeff);
    madStructOffset(a, b, &LiquidColors::farRiverColor    , blendCoeff);
    madStructOffset(a, b, &LiquidColors::closeOceanColor  , blendCoeff);
    madStructOffset(a, b, &LiquidColors::farOceanColor    , blendCoeff);

    madStructOffset(a, b, &LiquidColors::riverShallowAlpha , blendCoeff);
    madStructOffset(a, b, &LiquidColors::riverDeepAlpha    , blendCoeff);
    madStructOffset(a, b, &LiquidColors::oceanShallowAlpha , blendCoeff);
    madStructOffset(a, b, &LiquidColors::oceanDeepAlpha    , blendCoeff);
}
template <>
void madStructure<SkyBodyData>(SkyBodyData& a, SkyBodyData& b, float blendCoeff) {
    madStructOffset(a, b, &SkyBodyData::celestialBodyOverride  , blendCoeff);
    madStructOffset(a, b, &SkyBodyData::celestialBodyOverride2 , blendCoeff);
}

void DayNightLightHolder::updateLightAndSkyboxData(const HMapRenderPlan &mapRenderPlan,
                                                   MathHelper::FrustumCullingData &frustumData,
                                                   StateForConditions &stateForConditions,
                                                   const AreaRecord &areaRecord) {

    ZoneScoped ;
    if(!m_api) return;

    Config* config = this->m_api->getConfig();

    bool fogRecordWasFound = false;
    mathfu::vec3 endFogColor = mathfu::vec3(0.0, 0.0, 0.0);

    std::vector<SMOFog_Data> wmoFogData = {};
    if (mapRenderPlan->m_currentWMO != emptyWMO) {
        auto l_currentWmoObject = wmoFactory->getObjectById<0>(mapRenderPlan->m_currentWMO);
        if (l_currentWmoObject != nullptr) {
            l_currentWmoObject->checkFog(frustumData.cameraPos, wmoFogData);
        }
    }

    FogResult exteriorFogResult;

    auto fdd = mapRenderPlan->frameDependentData;
    if ((m_api->databaseHandler != nullptr)) {
        //Check zoneLight
        SkyColors skyColors;
        ExteriorColors exteriorColors;
        float currentGlow = 0.0f;

        LiquidColors liquidColors;
        SkyBodyData skyBodyData;

        SkyBoxCollector skyBoxCollector(m_api, m_exteriorSkyBoxes);

        getLightResultsFromDB(frustumData.cameraPos, config,
                              currentGlow,
                              skyColors, skyBodyData, exteriorColors,
                              exteriorFogResult, liquidColors, skyBoxCollector, &stateForConditions);

        m_exteriorSkyBoxes = skyBoxCollector.getNewSkyBoxes();
        mapRenderPlan->frameDependentData->overrideValuesWithFinalFog = skyBoxCollector.getOverrideValuesWithFinalFog();

        {
            mathfu::vec3 sunPlanetPosVec3 = MathHelper::calcSunPlanetPos(
                mapRenderPlan->renderingMatrices->lookAtMat,
                m_api->getConfig()->currentTime
            ) + frustumData.cameraPos;

            if (skyBodyData.celestialBodyOverride2.LengthSquared() > 0.0f) {
                sunPlanetPosVec3 = mathfu::vec3(
                    skyBodyData.celestialBodyOverride2[0],
                    skyBodyData.celestialBodyOverride2[1],
                    skyBodyData.celestialBodyOverride2[2]);
            }
            mathfu::vec4 sunPlanetPos = mathfu::vec4((sunPlanetPosVec3 - frustumData.cameraPos).Normalized(), 0.0f);
            fdd->sunDirection = (frustumData.viewMat.Inverse().Transpose() * sunPlanetPos).xyz().Normalized();
        }

        float ambientMult = areaRecord.ambientMultiplier * 2.0f + 1;

        if (config->glowSource == EParameterSource::eDatabase) {
            auto fdd = mapRenderPlan->frameDependentData;
            fdd->currentGlow = currentGlow;
        } else if (config->glowSource == EParameterSource::eConfig) {
            auto fdd = mapRenderPlan->frameDependentData;
            fdd->currentGlow = config->currentGlow; //copy from config to FDD
        }


        if (config->globalLighting == EParameterSource::eDatabase) {
            auto fdd = mapRenderPlan->frameDependentData;

            fdd->colors = exteriorColors;

            auto extDir = MathHelper::calcExteriorColorDir(
                frustumData.viewMat,
                m_api->getConfig()->currentTime
            );
            fdd->exteriorDirectColorDir = { extDir.x, extDir.y, extDir.z };
        } else if (config->globalLighting == EParameterSource::eConfig) {
            auto fdd = mapRenderPlan->frameDependentData;

            fdd->colors = config->exteriorColors;

            auto extDir = MathHelper::calcExteriorColorDir(
                frustumData.viewMat,
                m_api->getConfig()->currentTime
            );
            fdd->exteriorDirectColorDir = { extDir.x, extDir.y, extDir.z };
        }

        {
            auto fdd = mapRenderPlan->frameDependentData;
            fdd->useMinimapWaterColor = config->useMinimapWaterColor;
            fdd->useCloseRiverColorForDB = config->useCloseRiverColorForDB;
        }
        if (config->waterColorParams == EParameterSource::eDatabase)
        {
            auto fdd = mapRenderPlan->frameDependentData;
            fdd->liquidColors = liquidColors;
        } else if (config->waterColorParams == EParameterSource::eConfig) {
            auto fdd = mapRenderPlan->frameDependentData;
            fdd->liquidColors = config->liquidColors;
        }
        if (config->skyParams == EParameterSource::eDatabase) {
            auto fdd = mapRenderPlan->frameDependentData;
            fdd->skyColors = skyColors;
        } else if (config->skyParams == EParameterSource::eConfig) {
            auto fdd = mapRenderPlan->frameDependentData;
            fdd->skyColors = config->skyColors;
        }
    }

    //Handle fog
    {
        std::vector<LightResult> combinedResults = {};
        float totalSummator = 0.0;

        //Apply fog from WMO
//        {
//            for (auto &wmoFog : wmoFogData) {
//                auto &lightResult = combinedResults.emplace_back();
//                auto farPlaneClamped = std::min<float>(config->farPlane, wmoFog.end);
//
//                std::array<float, 3> colorConverted;
//                ImVectorToArrBGR(colorConverted, wmoFog.color);
//
//                lightResult.FogEnd = farPlaneClamped;
//                lightResult.FogStart = farPlaneClamped * wmoFog.start_scalar;
//                lightResult.SkyFogColor = colorConverted;
//                lightResult.FogDensity = 1.0;
//                lightResult.FogHeightColor = colorConverted;
//                lightResult.EndFogColor = colorConverted;
//                lightResult.SunFogColor = colorConverted;
//                lightResult.HeightEndFogColor = colorConverted;
//
//                if (farPlaneClamped < 30.f) {
//                    lightResult.FogEnd = farPlaneClamped;
//                    farPlaneClamped = 30.f;
//                }
//
//                bool mapHasWeightedBlendFlag = false;
//                if (!mapHasWeightedBlendFlag) {
//                    float difference = farPlaneClamped - lightResult.FogStart;
//                    float farPlaneClamped2 = std::min<float>(config->farPlane, 700.0f) - 200.0f;
//                    if ((difference > farPlaneClamped2) || (farPlaneClamped2 <= 0.0f)) {
//                        lightResult.FogDensity = 1.5;
//                    } else {
//                        lightResult.FogDensity = ((1.0 - (difference / farPlaneClamped2)) * 5.5) + 1.5;
//                    }
//                    lightResult.FogEnd = config->farPlane;
//                    if (lightResult.FogStart < 0.0f)
//                        lightResult.FogStart = 0.0;
//                }
//
//                lightResult.FogHeightDensity = lightResult.FogDensity;
//                lightResult.FogStartOffset = 0;
//                lightResult.FogHeightScaler = 1.0;
//                lightResult.FogZScalar = 0;
//                lightResult.FogHeight = -10000.0;
//                lightResult.LegacyFogScalar = 1.0;
//                lightResult.EndFogColorDistance = 10000.0;
//            }
//        }

        //In case of no data -> disable the fog
        {
            auto fdd = mapRenderPlan->frameDependentData;
            fdd->FogDataFound = !combinedResults.empty();

            auto &fogResult = fdd->fogResults.emplace_back();
            if (config->globalFog == EParameterSource::eDatabase) {
                fogResult = exteriorFogResult;
            } else if (config->globalFog == EParameterSource::eConfig){
                fogResult = config->fogResult;
            }

            fdd->FogDataFound = true;

        }
    }
}


template <int T>
inline float getFloatFromInt(int value) {
    if constexpr (T == 0) {
        return (value & 0xFF) / 255.0f;
    }
    if constexpr (T == 1) {
        return ((value >> 8) & 0xFF) / 255.0f;
    }
    if constexpr (T == 2) {
        return ((value >> 16) & 0xFF) / 255.0f;
    }

    return 0.0f;
}

inline mathfu::vec3 intToColor3(int a) {
    //BGR
    return mathfu::vec3(
        getFloatFromInt<2>(a),
        getFloatFromInt<1>(a),
        getFloatFromInt<0>(a)
    );
}
inline mathfu::vec4 intToColor4(int a) {
    //BGRA
    return mathfu::vec4(
        getFloatFromInt<2>(a),
        getFloatFromInt<1>(a),
        getFloatFromInt<0>(a),
        getFloatFromInt<3>(a)
    );
}
inline mathfu::vec4 floatArr(std::array<float, 4> a) {
    //BGRA
    return mathfu::vec4(
        a[3],
        a[2],
        a[1],
        a[0]
    );
}

template <int C, typename T>
decltype(auto) mixMembers(LightParamData& data, T LightTimedData::*member, float blendTimeCoeff) {
    if constexpr (C == 3) {
        static_assert(std::is_same<T, int>::value, "the type must be int for vector component");
        return mix(intToColor3(data.lightTimedData[0].*member), intToColor3(data.lightTimedData[1].*member), blendTimeCoeff);
    }
    if constexpr (C == 4 && std::is_same<T, std::array<float, 4>>::value) {
        return mix(floatArr(data.lightTimedData[0].*member), floatArr(data.lightTimedData[1].*member), blendTimeCoeff);
    } else if constexpr (C == 4) {
        static_assert(std::is_same<T, int>::value, "the type must be int for vector component");
        return mix(intToColor4(data.lightTimedData[0].*member), intToColor4(data.lightTimedData[1].*member), blendTimeCoeff);
    }
    if constexpr (C == 1) {
        static_assert(std::is_same<T, float>::value, "the type must be float for one component");
        return mix(data.lightTimedData[0].*member, data.lightTimedData[1].*member, blendTimeCoeff);
    }
}

bool vec3EqZero(const mathfu::vec3 &a) {
    return feq(a.x, 0.0f) && feq(a.y, 0.0f) && feq(a.z, 0.0f);
}

float maxFarClip(float farClip) {
     return std::max<float>(std::min<float>(farClip, 50000.0), 1000.0);
}

float DayNightLightHolder::getClampedFarClip(float farClip) {
    float multiplier = 1.0f;

    if ((m_mapHasFlag_0x10000) != 0 && farClip >= 4400.0f)
        farClip = 4400.0;

    return maxFarClip(fmaxf(fmaxf(fmaxf(m_minFogDist1, farClip), m_minFogDist3), m_minFogDist2));
}

void DayNightLightHolder::fixLightTimedData(LightTimedData &data, float farClip, float &fogScalarOverride) {
    if (data.EndFogColor == 0) {
        data.EndFogColor = data.SkyFogColor;
    }

    if (data.FogHeightColor == 0) {
        data.FogHeightColor = data.SkyFogColor;
    }

    if (data.EndFogHeightColor == 0) {
        data.EndFogHeightColor = data.EndFogColor;
    }

    //Clamp into (-1.0f, 1.0f)
    data.FogScaler = std::max<float>(std::min<float>(data.FogScaler, 1.0f), -1.0f);
    data.FogEnd = std::max<float>(data.FogEnd, 10.0f);
    data.FogHeight = std::max<float>(data.FogHeight, -10000.0f);
    data.FogHeightScaler = std::max<float>(std::min<float>(data.FogHeightScaler, 1.0f), -1.0f);

    if (data.SunFogColor == 0)
        data.SunFogAngle = 1.0f;

    if (data.EndFogColorDistance <= 0.0f)
        data.EndFogColorDistance = getClampedFarClip(farClip);

    if (data.FogHeight > 10000.0f)
        data.FogHeight = 0.0f;

    if (data.FogDensity <= 0.0f) {
        float farPlaneClamped = std::min<float>(farClip, 700.0f) - 200.0f;

        float difference = data.FogEnd - (float)(data.FogEnd * data.FogScaler);
        if (difference > farPlaneClamped || difference <= 0.0f) {
            data.FogDensity = 1.5f;
        } else {
            data.FogDensity = ((1.0f - (difference / farPlaneClamped)) * 5.5f) + 1.5f;
        }
    } else {
        fogScalarOverride = std::min(fogScalarOverride, -0.2f);
    }

    if ( data.FogHeightScaler == 0.0f )
        data.FogHeightDensity = data.FogDensity;
}

void DayNightLightHolder::getLightResultsFromDB(mathfu::vec3 &cameraVec3, const Config *config,
                                float &glow,
                                SkyColors &skyColors,
                                SkyBodyData &skyBodyData,
                                ExteriorColors &exteriorColors,
                                FogResult &fogResult,
                                LiquidColors &liquidColors,
                                SkyBoxCollector &skyBoxCollector,
                                StateForConditions *stateForConditions) {
    if (!m_api || !m_api->databaseHandler)
        return ;

    int currentLightParamIdIndex = 0;
    auto paramsBlend = calculateLightParamBlends(
        m_api->databaseHandler,
        m_mapId,
        cameraVec3,
        stateForConditions,
        m_zoneLights,
        currentLightParamIdIndex
    );


    if (!paramsBlend.empty()) {
        // assign zero values
        skyColors.assignZeros();
        exteriorColors.assignZeros();
        liquidColors.assignZeros();
        skyBodyData.assignZeros();
        //fogResult.assignZeros();
    }

    for (auto it = paramsBlend.begin(); it != paramsBlend.end(); it++) {
        SkyColors tmp_skyColors;
        ExteriorColors tmp_exteriorColors;

        LiquidColors tmp_liquidColors;
        SkyBodyData tmp_skyBodyData;
        FogResult tmp_fogResult;

        float tmp_glow = 0.0;

        calcLightParamResult(it->id, config,
                             tmp_glow,
                             tmp_skyBodyData, tmp_exteriorColors, tmp_fogResult, tmp_liquidColors, tmp_skyColors);

        mixStructure(skyColors,      tmp_skyColors,      it->blend);
        mixStructure(exteriorColors, tmp_exteriorColors, it->blend);
        mixStructure(liquidColors,   tmp_liquidColors,   it->blend);
        mixStructure(skyBodyData,    tmp_skyBodyData,    it->blend);
        mixStructure(fogResult,      tmp_fogResult,      it->blend);
        glow =   mix(glow,           tmp_glow,           it->blend);

        if (tmp_skyBodyData.skyBoxInfo.id > 0 && (stateForConditions != nullptr)) {
            skyBoxCollector.addSkyBox(*stateForConditions, tmp_skyBodyData.skyBoxInfo, it->blend);
        }
    }


    float blendCoeff = fmaxf(fminf(getClampedFarClip(config->farPlane) / fogResult.EndFogColorDistance, 1.0f), 0.0f);
    skyColors.SkyFogColor = mix(skyColors.SkyFogColor, fogResult.EndFogColor, blendCoeff);

    stateForConditions->currentLightParams = paramsBlend;
}

void DayNightLightHolder::calcLightParamResult(int lightParamId, const Config *config,
                                               float &glow,
                                               SkyBodyData &skyBodyData,
                                               ExteriorColors &exteriorColors, FogResult &fogResult,
                                               LiquidColors &liquidColors,
                                               SkyColors &skyColors) {
    LightParamData lightParamData;
    if (m_api->databaseHandler->getLightParamData(lightParamId, config->currentTime, lightParamData)) {

        bool resultTimeIsTheSame = lightParamData.lightTimedData[1].time == lightParamData.lightTimedData[0].time;
        float blendTimeCoeff = 0.0f;
        if (!resultTimeIsTheSame)
            blendTimeCoeff =
                (float)(config->currentTime - lightParamData.lightTimedData[0].time) /
                (float)(lightParamData.lightTimedData[1].time - lightParamData.lightTimedData[0].time);

        blendTimeCoeff = std::min<float>(std::max<float>(blendTimeCoeff, 0.0f), 1.0f);

        skyBodyData.skyBoxInfo = lightParamData.skyboxInfo;
        skyBodyData.celestialBodyOverride2 = mathfu::vec3(
            lightParamData.celestialBodyOverride2[0],
            lightParamData.celestialBodyOverride2[1],
            lightParamData.celestialBodyOverride2[2]
        );

        glow = lightParamData.glow;

        auto &dataA = lightParamData.lightTimedData[0];
        auto &dataB = lightParamData.lightTimedData[1];

        //Blend two times using certain rules
        float fogScalarOverride = 0.0f;
        fixLightTimedData(dataA, config->farPlane, fogScalarOverride);
        fixLightTimedData(dataB, config->farPlane, fogScalarOverride);


        //Ambient lights
        exteriorColors.exteriorAmbientColor =         mixMembers<3>(lightParamData, &LightTimedData::ambientLight, blendTimeCoeff);
        exteriorColors.exteriorGroundAmbientColor =   mixMembers<3>(lightParamData, &LightTimedData::groundAmbientColor, blendTimeCoeff);
        if (vec3EqZero(exteriorColors.exteriorGroundAmbientColor))
            exteriorColors.exteriorGroundAmbientColor = exteriorColors.exteriorAmbientColor;

        exteriorColors.exteriorHorizontAmbientColor = mixMembers<3>(lightParamData, &LightTimedData::horizontAmbientColor, blendTimeCoeff);
        if (vec3EqZero(exteriorColors.exteriorHorizontAmbientColor))
            exteriorColors.exteriorHorizontAmbientColor = exteriorColors.exteriorAmbientColor;

        exteriorColors.exteriorDirectColor =          mixMembers<3>(lightParamData, &LightTimedData::directColor, blendTimeCoeff);

        //Liquid colors
        liquidColors.closeOceanColor = mixMembers<3>(lightParamData, &LightTimedData::closeOceanColor, blendTimeCoeff);
        liquidColors.farOceanColor =   mixMembers<3>(lightParamData, &LightTimedData::farOceanColor, blendTimeCoeff);
        liquidColors.closeRiverColor = mixMembers<3>(lightParamData, &LightTimedData::closeRiverColor, blendTimeCoeff);
        liquidColors.farRiverColor =   mixMembers<3>(lightParamData, &LightTimedData::farRiverColor, blendTimeCoeff);

        liquidColors.oceanShallowAlpha = lightParamData.oceanShallowAlpha;
        liquidColors.oceanDeepAlpha    = lightParamData.oceanDeepAlpha;
        liquidColors.riverShallowAlpha = lightParamData.waterShallowAlpha;
        liquidColors.riverDeepAlpha    = lightParamData.waterDeepAlpha;

        //SkyColors
        skyColors.SkyTopColor =    mixMembers<3>(lightParamData, &LightTimedData::SkyTopColor, blendTimeCoeff);
        skyColors.SkyMiddleColor = mixMembers<3>(lightParamData, &LightTimedData::SkyMiddleColor, blendTimeCoeff);
        skyColors.SkyBand1Color =  mixMembers<3>(lightParamData, &LightTimedData::SkyBand1Color, blendTimeCoeff);
        skyColors.SkyBand2Color =  mixMembers<3>(lightParamData, &LightTimedData::SkyBand2Color, blendTimeCoeff);
        skyColors.SkySmogColor =   mixMembers<3>(lightParamData, &LightTimedData::SkySmogColor, blendTimeCoeff);
        skyColors.SkyFogColor =    mixMembers<3>(lightParamData, &LightTimedData::SkyFogColor, blendTimeCoeff);

        //Fog!
        fogResult.FogEnd =           mixMembers<1>(lightParamData, &LightTimedData::FogEnd, blendTimeCoeff);
        fogResult.FogScaler =        mixMembers<1>(lightParamData, &LightTimedData::FogScaler, blendTimeCoeff);
        fogResult.FogDensity =       mixMembers<1>(lightParamData, &LightTimedData::FogDensity, blendTimeCoeff);
        fogResult.FogHeight =        mixMembers<1>(lightParamData, &LightTimedData::FogHeight, blendTimeCoeff);
        fogResult.FogHeightScaler =  mixMembers<1>(lightParamData, &LightTimedData::FogHeightScaler, blendTimeCoeff);
        fogResult.FogHeightDensity = mixMembers<1>(lightParamData, &LightTimedData::FogHeightDensity, blendTimeCoeff);
        //Custom blend for Sun
        {
            float SunFogAngle1 = lightParamData.lightTimedData[0].SunFogAngle;
            float SunFogAngle2 = lightParamData.lightTimedData[1].SunFogAngle;
            if (SunFogAngle1 >= 1.0f) {
                if (SunFogAngle2 >= 1.0f) {
                    fogResult.SunAngleBlend = 0.0f;
                    fogResult.SunFogStrength = 0.0f;
                    fogResult.SunAngleBlend = 1.0f;
                } else if (SunFogAngle2 < 1.0f) {
                    fogResult.SunAngleBlend = blendTimeCoeff;
                    fogResult.SunFogAngle = SunFogAngle2;
                    fogResult.SunFogStrength = lightParamData.lightTimedData[1].SunFogStrength;
                }
            } else if (SunFogAngle1 < 1.0f) {
                if (SunFogAngle2 < 1.0f) {
                    fogResult.SunAngleBlend = 1.0f;
                    fogResult.SunFogAngle =      mixMembers<1>(lightParamData, &LightTimedData::SunFogAngle, blendTimeCoeff);
                    fogResult.SunFogStrength =   mixMembers<1>(lightParamData, &LightTimedData::SunFogStrength, blendTimeCoeff);
                } else if (SunFogAngle2 >= 1.0f) {
                    fogResult.SunFogStrength = lightParamData.lightTimedData[0].SunFogStrength;
                    fogResult.SunFogAngle = SunFogAngle1;
                    fogResult.SunAngleBlend = 1.0f - blendTimeCoeff;
                }
            }
        }

        if (false) {//fdd->overrideValuesWithFinalFog) {
            fogResult.FogColor = mixMembers<3>(lightParamData, &LightTimedData::EndFogColor, blendTimeCoeff);
        } else {
            fogResult.FogColor = mixMembers<3>(lightParamData, &LightTimedData::SkyFogColor, blendTimeCoeff);
        }

        fogResult.EndFogColor =           mixMembers<3>(lightParamData, &LightTimedData::EndFogColor, blendTimeCoeff);
        fogResult.EndFogColorDistance =   mixMembers<1>(lightParamData, &LightTimedData::EndFogColorDistance, blendTimeCoeff);
        fogResult.SunFogColor =           mixMembers<3>(lightParamData, &LightTimedData::SunFogColor, blendTimeCoeff);
//        fogResult.SunFogColor = mathfu::vec3(0,0,0);
        fogResult.FogHeightColor =        mixMembers<3>(lightParamData, &LightTimedData::FogHeightColor, blendTimeCoeff);
        fogResult.FogHeightCoefficients = mixMembers<4>(lightParamData, &LightTimedData::FogHeightCoefficients, blendTimeCoeff);
        fogResult.MainFogCoefficients =   mixMembers<4>(lightParamData, &LightTimedData::MainFogCoefficients, blendTimeCoeff);
        fogResult.HeightDensityFogCoefficients = mixMembers<4>(lightParamData, &LightTimedData::MainFogCoefficients, blendTimeCoeff);

        fogResult.FogZScalar =        mixMembers<1>(lightParamData, &LightTimedData::FogZScalar, blendTimeCoeff);
        fogResult.MainFogStartDist =  mixMembers<1>(lightParamData, &LightTimedData::MainFogStartDist, blendTimeCoeff);
        fogResult.MainFogEndDist =    mixMembers<1>(lightParamData, &LightTimedData::MainFogEndDist, blendTimeCoeff);
        fogResult.HeightEndFogColor = mixMembers<3>(lightParamData, &LightTimedData::EndFogHeightColor, blendTimeCoeff);
        fogResult.FogStartOffset =    mixMembers<1>(lightParamData, &LightTimedData::FogStartOffset, blendTimeCoeff);

        if (fogResult.FogHeightCoefficients.LengthSquared() <= 0.00000011920929f ){
            fogResult.FogHeightCoefficients = mathfu::vec4(0,0,1,0);
        }

        if (
            (fogResult.MainFogCoefficients.LengthSquared()) > 0.00000011920929f ||
            (fogResult.HeightDensityFogCoefficients.LengthSquared()) > 0.00000011920929f
            ) {
            fogResult.LegacyFogScalar = 0.0f;
        } else {
            fogResult.LegacyFogScalar = 1.0f;
        }

        fogResult.FogDensity = fmaxf(fogResult.FogDensity, 0.89999998f);
        if (m_mapFlag2_0x2)
        {
            fogResult.FogDensity = 1.0f;
        }
        else if ( fogScalarOverride > fogResult.FogScaler )
        {
            fogResult.FogScaler = fogScalarOverride;
        }
    } else {
        // lightParamData.lightTimedData[0].time = 0;
    }
}

void DayNightLightHolder::createMinFogDistances() {
    m_minFogDist1 = maxFarClip(0.0f);
    m_minFogDist2 = maxFarClip(0.0f);

    switch ( m_mapId )
    {
        case 2695:
            m_minFogDist1 = maxFarClip(7000.0);
            break;
        case 1492:
            m_minFogDist1 = maxFarClip(7000.0);
            break;
        case 1718:
            m_minFogDist1 = maxFarClip(7000.0);
            break;
        case 571:
            m_minFogDist1 = maxFarClip(30000.0);
            break;
    }

    if (m_mapHasFlag_0x200000) {
        m_minFogDist2 = maxFarClip(30000.0f);
    }
}

void DayNightLightHolder::SkyBoxCollector::addSkyBox(StateForConditions &stateForConditions, const SkyBoxInfo &skyBoxInfo, float alpha) {
    if (skyBoxInfo.skyBoxFdid == 0) return;

    std::shared_ptr<M2Object> skyBoxModel = nullptr;

    //1. Find in added skyboxes
    for (const auto &model : m_newSkyBoxes) {
        if (skyBoxInfo.skyBoxFdid == model->getModelFileId()) {
            skyBoxModel = model;

            float currentAlpha = model->getAlpha();
            model->setAlpha(std::max<float>(currentAlpha, alpha));
            break;
        }
    }

    //2. Otherwise, find in old skyboxes
    if (!skyBoxModel) {
        for (const auto &model: m_existingSkyBoxes) {
            if (skyBoxInfo.skyBoxFdid == model->getModelFileId()) {
                skyBoxModel = model;

                model->setAlpha(alpha);
                m_newSkyBoxes.push_back(model);
                break;
            }
        }
    }

    stateForConditions.currentSkyboxIds.push_back({skyBoxInfo.id, alpha});

    //3. Otherwise create
    if (skyBoxModel == nullptr) {
        skyBoxModel = m2Factory->createObject(m_api, true);
        skyBoxModel->setLoadParams(0, {}, {});

        skyBoxModel->setModelFileId(skyBoxInfo.skyBoxFdid);

        skyBoxModel->createPlacementMatrix(mathfu::vec3(0, 0, 0), 0, mathfu::vec3(1, 1, 1), nullptr);
        skyBoxModel->calcWorldPosition();
        m_newSkyBoxes.push_back(skyBoxModel);
    }


    if ((skyBoxInfo.skyBoxFlags & 4) > 0 ) {
        //In this case cone is still rendered been, but all values are final fog values.
        m_overrideValuesWithFinalFog = true;
    }

    if (skyBoxInfo.skyBoxFlags & 1) {
        skyBoxModel->setOverrideAnimationPerc(m_api->getConfig()->currentTime / 2880.0, true);
    }
}
