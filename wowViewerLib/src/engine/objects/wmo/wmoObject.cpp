//
// Created by deamon on 03.07.17.
//

#include "wmoObject.h"
#include "../../algorithms/mathHelper.h"
#include "../../algorithms/grahamScan.h"
#include "../../persistance/header/commonFileStructs.h"
#include "./../../../gapi/interface/IDevice.h"
#include "../../../renderer/frame/FrameProfile.h"
#include <algorithm>

std::vector<mathfu::vec3> createAntiPortal(const HWmoGroupGeom& groupGeom, const mathfu::mat4 &placementMat)
{
    std::vector<mathfu::vec3> points(0);

    if (groupGeom == nullptr || groupGeom->getStatus() != FileStatus::FSLoaded) return points;
    for ( unsigned int mopy_index (0), movi_index (0)
            ; mopy_index < groupGeom->mopyLen
            ; ++mopy_index, ++movi_index
            )
    {
        points.emplace_back() = (placementMat * mathfu::vec4(mathfu::vec3(groupGeom->verticles[groupGeom->indicies[3*movi_index+0]]), 1.0f)).xyz();
        points.emplace_back() = (placementMat * mathfu::vec4(mathfu::vec3(groupGeom->verticles[groupGeom->indicies[3*movi_index+1]]), 1.0f)).xyz();
        points.emplace_back() = (placementMat * mathfu::vec4(mathfu::vec3(groupGeom->verticles[groupGeom->indicies[3*movi_index+2]]), 1.0f)).xyz();
    }
    return points;
}


void WmoObject::startLoading() {
    if (!m_loading) {
        m_loading = true;


        Cache<WmoMainGeom> *wmoGeomCache = m_api->cacheStorage->getWmoMainCache();
        if (!useFileId) {
            mainGeom = wmoGeomCache->get(m_modelName);
        } else {
            mainGeom = wmoGeomCache->getFileId(m_modelFileId);
        }

    }
}

std::shared_ptr<M2Object> WmoObject::getDoodad(int index, int fromGroupIndex) {
    auto iterator = this->m_doodadsUnorderedMap.find(index);
    if (iterator != this->m_doodadsUnorderedMap.end())
        return iterator->second;

    bool existsInActiveDoodadSets = false;
    for (int i = 0; i < this->mainGeom->doodadSetsLen; i++) {
        if (!m_activeDoodadSets[i]) continue;;

        if (
            index >= this->mainGeom->doodadSets[i].firstinstanceindex &&
            index <= this->mainGeom->doodadSets[i].firstinstanceindex + this->mainGeom->doodadSets[i].numDoodads
            ) {
            existsInActiveDoodadSets = true;
            break;
        }
    }

    if (!existsInActiveDoodadSets)
        return nullptr;

    const SMODoodadDef *doodadDef = &this->mainGeom->doodadDefs[index];

    bool fileIdMode = false;
    int doodadfileDataId = 0;
    std::string fileName;
    if (this->mainGeom->doodadFileDataIds == nullptr && this->mainGeom->doodadFileDataIdsLen == 0) {
        if (this->mainGeom->doodadNamesFieldLen <= 0) {
            return nullptr;
        }
        assert(doodadDef->name_offset < this->mainGeom->doodadNamesFieldLen);
        fileName = std::string (&this->mainGeom->doodadNamesField[doodadDef->name_offset]);
    } else {
        doodadfileDataId = this->mainGeom->doodadFileDataIds[doodadDef->name_offset];
        fileIdMode = true;
    }

    auto m2Object = m2Factory->createObject(m_api);

    m2Object->setLoadParams(0, {},{});

    //Initial state for doodads in WMO is interior lighting. Unless there is exterior group WMO that references it
    m2Object->setInteriorExteriorBlend(0.0f);
    if (fileIdMode) {
        m2Object->setModelFileId(doodadfileDataId);
    } else {
        m2Object->setModelFileName(fileName);
    }
    m2Object->createPlacementMatrix(*doodadDef, m_placementMatrix);
    m2Object->calcWorldPosition();

    //Set interior lighting params
    float mddiVal = 1.0f;
    if (doodadDef->flag_0x10 && index < mainGeom->m_doodadAdditionalInfoLen) {
        mddiVal = mainGeom->m_doodadAdditionalInfo[index];
    }

    auto l_wmoId = this->getObjectId();
    m2Object->addPostLoadEvent([l_wmoId, l_doodadDef = *doodadDef, m2Object, mddiVal, fromGroupIndex]() {
        auto wmoObj = wmoFactory->getObjectById<0>(l_wmoId);
        if (!wmoObj) return;

        auto wmoFlag = wmoObj->mainGeom->groups[fromGroupIndex].flags;
        if (wmoFlag.EXTERIOR_LIT || wmoFlag.EXTERIOR || !wmoFlag.INTERIOR) return;

        wmoObj->applyLightingParamsToDoodad(&l_doodadDef, m2Object, mddiVal, fromGroupIndex);
    });

    // if (doodadDef->flag_AcceptProjTex) std::cout << "doodadDef->flag_AcceptProjTex" << std::endl;
    // if (doodadDef->flag_0x2) std::cout << "doodadDef->flag_0x2" << std::endl;
    // if (doodadDef->flag_0x4) std::cout << "doodadDef->flag_0x4" << std::endl;
    // if (doodadDef->flag_0x8) std::cout << "doodadDef->flag_0x8" << std::endl;
    // if (doodadDef->flag_0x10) std::cout << "doodadDef->flag_0x10" << std::endl;
    // if (doodadDef->flag_0x20) std::cout << "doodadDef->flag_0x20" << std::endl;
    // if (doodadDef->flag_0x40) std::cout << "doodadDef->flag_0x40" << std::endl;
    // if (doodadDef->flag_0x80) std::cout << "doodadDef->flag_0x80" << std::endl;

    this->m_doodadsUnorderedMap[index] = m2Object;

    return m2Object;
}

mathfu::vec3 addDirectColorAndAmbient(mathfu::vec3 directColor, mathfu::vec3 ambient) {
    auto sum = ambient + directColor;

    float biggestComponent = std::max(sum.x, std::max(sum.y, sum.z));
    if (biggestComponent > 1.0f) {
        sum = sum * (1.0f / biggestComponent);
    }
    sum = mathfu::vec3(
        fminf(sum.x, 1.0f),
        fminf(sum.y, 1.0f),
        fminf(sum.z, 1.0f)
    );
    return sum;
}

inline mathfu::vec3 fixDirectColor(const mathfu::vec3 &directColor, uint8_t limit) {
    float biggestComponent = std::max(directColor.x, std::max(directColor.y, directColor.z));

    if (biggestComponent <= 0.00001f) {
        biggestComponent = 1.0f/255.0f;
    }

    const float limitInFloat = limit * (1.0f/255.0f);
    if (biggestComponent < limitInFloat) {
        auto hsv = MathHelper::rgb2hsv(directColor);
        hsv.v = hsv.v * (limitInFloat / biggestComponent);
        auto rgb = MathHelper::hsv2rgb(hsv);
        return rgb;
    }

    return directColor;
}
inline mathfu::vec3 fixAmbient1(const mathfu::vec3 &ambient, uint8_t limit) {
    float biggestComponent = std::max(ambient.x, std::max(ambient.y, ambient.z));

    if (biggestComponent <= 0.00001f) {
        biggestComponent = 1.0f/255.0f;
    }

    const float limitInFloat = limit * (1.0f/255.0f);
    if (biggestComponent > limitInFloat) {
        return ambient * (limitInFloat / biggestComponent);
    }

    return ambient;
}

void WmoObject::applyColorFromMOLT(
        const SMODoodadDef *doodadDef,
        const std::shared_ptr<M2Object> &doodad,
        std::array<mathfu::vec3, 3> &interiorAmbients,
        mathfu::vec3 &color,
        bool &hasDoodad0x4Flag,
        int fromGroupIndex)
{

    hasDoodad0x4Flag = doodadDef->flag_0x4;
    if (doodadDef->flag_0x40)
        hasDoodad0x4Flag = true;

    if (doodadDef->flag_0x8) {
        interiorAmbients[0] = color;
        interiorAmbients[1] = color;
        interiorAmbients[2] = color;

        doodad->setAmbientColorOverride(
            interiorAmbients[0],
            interiorAmbients[2],
            interiorAmbients[1]
        );
    }

    if (hasDoodad0x4Flag) {
        if (doodadDef->flag_0x40) {
            color = mathfu::vec3(0,0,0);
            doodad->setInteriorDirectColor(color);
        } else if (doodadDef->color.a != 255 && doodadDef->color.a < this->mainGeom->lightsLen) {
            auto &light = this->mainGeom->lights[doodadDef->color.a];

            auto MOLTWorldPos = this->m_placementMatrix * mathfu::vec4(mathfu::vec3(light.position), 1.0f);

            const auto doodadAAbb = doodad->getBoundingBox();

            auto sunDirVec = (mathfu::vec3(doodadAAbb.max) + mathfu::vec3(doodadAAbb.min)) * 0.5f - MOLTWorldPos.xyz();
            if (sunDirVec.LengthSquared() > 0) {
                doodad->setSunDirOverride(sunDirVec.Normalized());
            }

            auto lightColor = ImVectorToVec4(light.color).xyz() * light.intensity;
            float biggestComponent = std::max(lightColor.x, std::max(lightColor.y, lightColor.z));
            if (biggestComponent > 1.0f) {
                float inv = 1.0f / biggestComponent;
                lightColor = lightColor * inv;
                color = mathfu::vec3(
                    fminf(lightColor.x, 1.0f),
                    fminf(lightColor.y, 1.0f),
                    fminf(lightColor.z, 1.0f)
                );
            }

            if (doodadDef->flag_0x2 == 0) {
                auto directColor = color;
                directColor = fixDirectColor(directColor, 0x70);
                doodad->setInteriorDirectColor(directColor);
            }
        }
    } else {
        if (!doodadDef->flag_0x2 && false)
            return;
    }

    mathfu::vec3 interiorLightStart;
    if (doodadDef->color.a == 255) {
        auto const aabb = this->groupObjects[fromGroupIndex]->getWorldAABB();
        interiorLightStart = (mathfu::vec3(aabb.max) + mathfu::vec3(aabb.min)) * 0.5f;
    } else {
        auto &light = this->mainGeom->lights[doodadDef->color.a];

        interiorLightStart = this->m_placementMatrix * mathfu::vec4(mathfu::vec3(light.position), 1.0f).xyz();
    }

    const auto doodadAAbb = doodad->getBoundingBox();

    auto sunDirVec = (mathfu::vec3(doodadAAbb.max) + mathfu::vec3(doodadAAbb.min)) * 0.5f - interiorLightStart;

    if (sunDirVec.LengthSquared() > 0) {
        doodad->setSunDirOverride(sunDirVec.Normalized());
    }

}

void WmoObject::applyLightingParamsToDoodad(const SMODoodadDef *doodadDef, const std::shared_ptr<M2Object> &doodad, float mddiVal, int fromGroupIndex) {
    std::array<mathfu::vec3, 3> interiorAmbients;
    mathfu::Vector<float, 3> color;
    bool hasDoodad0x4Flag;

    interiorAmbients = getAmbientColors();
    color = ImVectorToVec4(doodadDef->color).xyz();

    applyColorFromMOLT(doodadDef, doodad, interiorAmbients, color, hasDoodad0x4Flag, fromGroupIndex);

    //If both flag_0x4 and flag_0x8 are set - nothing is applied
    if (hasDoodad0x4Flag && doodadDef->flag_0x8)
        return;

    if (doodadDef->flag_0x2 || true) { //TODO: Check this stupidity later
        if (doodadDef->flag_0x10 && !hasDoodad0x4Flag) {
            mathfu::vec3 directColor = mathfu::vec3(0,0,0);
            if (!doodadDef->flag_0x80) {
                mddiVal = std::max<float>(mddiVal, 1.0f);
                directColor = color * mddiVal;
            }
            color = addDirectColorAndAmbient(directColor,  interiorAmbients[0]);
        }
    } else {
        if (doodadDef->flag_0x10) {
            mathfu::vec3 directColor = mathfu::vec3(0,0,0);
            if (!doodadDef->flag_0x80) {
                mddiVal = std::max<float>(mddiVal, 1.0f);
                directColor = color * mddiVal;
            }

            interiorAmbients[0] = addDirectColorAndAmbient(directColor,  interiorAmbients[0]);
            interiorAmbients[1] = addDirectColorAndAmbient(directColor,  interiorAmbients[1]);
            interiorAmbients[2] = addDirectColorAndAmbient(directColor,  interiorAmbients[2]);
            color = interiorAmbients[0];
        } else {
            interiorAmbients[0] = color;
            interiorAmbients[1] = color;
            interiorAmbients[2] = color;
        }

        color = fixDirectColor(color, 0x70u);
        interiorAmbients[0] = fixAmbient1(interiorAmbients[0], 0x60u);
        interiorAmbients[1] = fixAmbient1(interiorAmbients[1], 0x60u);
        interiorAmbients[2] = fixAmbient1(interiorAmbients[2], 0x60u);
    }

    if (!hasDoodad0x4Flag) {
        doodad->setInteriorDirectColor(color);
    }
    if (!doodadDef->flag_0x8) {
        doodad->setAmbientColorOverride(
            interiorAmbients[0],
            interiorAmbients[2],
            interiorAmbients[1]
        );
    }


}

void WmoObject::createPlacementMatrix(const SMMapObjDef &mapObjDef){
    mathfu::mat4 adtToWorldMat4 = MathHelper::getAdtToWorldMat4();

    mathfu::mat4 placementMatrix = mathfu::mat4::Identity();
    placementMatrix *= adtToWorldMat4;
    placementMatrix *= mathfu::mat4::FromTranslationVector(mathfu::vec3(mapObjDef.position));
    placementMatrix *= mathfu::mat4::FromScaleVector(mathfu::vec3(-1, 1, -1));

    placementMatrix *= MathHelper::RotationY(toRadian(mapObjDef.rotation.y-270));
    placementMatrix *= MathHelper::RotationZ(toRadian(-mapObjDef.rotation.x));
    placementMatrix *= MathHelper::RotationX(toRadian(mapObjDef.rotation.z-90));
    if (mapObjDef.unk != 0) {
        placementMatrix *= mathfu::mat4::FromScaleVector(mathfu::vec3(mapObjDef.unk / 1024.0f));
    }
    mathfu::mat4 placementInvertMatrix = placementMatrix.Inverse();

    m_placementInvertMatrix = placementInvertMatrix;
    m_placementMatrix = placementMatrix;
    m_placementMatChanged = true;

    //BBox is in ADT coordinates. We need to transform it first
    const C3Vector &bb1 = mapObjDef.extents.min;
    const C3Vector &bb2 = mapObjDef.extents.max;
    mathfu::vec4 bb1vec = mathfu::vec4(bb1.x, bb1.y, bb1.z, 1);
    mathfu::vec4 bb2vec = mathfu::vec4(bb2.x, bb2.y, bb2.z, 1);

    CAaBox worldAABB = MathHelper::transformAABBWithMat4(
            adtToWorldMat4, bb1vec, bb2vec);

    createBB(worldAABB);
}
void WmoObject::createPlacementMatrix(const SMMapObjDefObj1 &mapObjDef){
    mathfu::mat4 adtToWorldMat4 = MathHelper::getAdtToWorldMat4();

    mathfu::mat4 placementMatrix = mathfu::mat4::Identity();
    placementMatrix *= adtToWorldMat4;
    placementMatrix *= mathfu::mat4::FromTranslationVector(mathfu::vec3(mapObjDef.position));
    placementMatrix *= mathfu::mat4::FromScaleVector(mathfu::vec3(-1, 1, -1));

    placementMatrix *= MathHelper::RotationY(toRadian(mapObjDef.rotation.y-270));
    placementMatrix *= MathHelper::RotationZ(toRadian(-mapObjDef.rotation.x));
    placementMatrix *= MathHelper::RotationX(toRadian(mapObjDef.rotation.z-90));

    if (mapObjDef.unk != 0) {
        placementMatrix *= mathfu::mat4::FromScaleVector(mathfu::vec3(mapObjDef.unk / 1024.0f));
    }

    mathfu::mat4 placementInvertMatrix = placementMatrix.Inverse();

    m_placementInvertMatrix = placementInvertMatrix;
    m_placementMatrix = placementMatrix;
    m_placementMatChanged = true;

    //BBox is in ADT coordinates. We need to transform it first
//    C3Vector &bb1 = mapObjDef.extents.min;
//    C3Vector &bb2 = mapObjDef.extents.max;
//    mathfu::vec4 bb1vec = mathfu::vec4(bb1.x, bb1.y, bb1.z, 1);
//    mathfu::vec4 bb2vec = mathfu::vec4(bb2.x, bb2.y, bb2.z, 1);
//
    mathfu::vec4 bb1vec = mathfu::vec4(-1000,-1000,-1000, 1);
    mathfu::vec4 bb2vec = mathfu::vec4(1000, 1000,1000, 1);

    CAaBox worldAABB = MathHelper::transformAABBWithMat4(
            adtToWorldMat4, bb1vec, bb2vec);

    createBB(worldAABB);
}

void WmoObject::createGroupObjects(){
    groupObjects = std::vector<std::shared_ptr<WmoGroupObject>>(mainGeom->groupsLen, nullptr);
    groupObjectsLod1 = std::vector<std::shared_ptr<WmoGroupObject>>(mainGeom->groupsLen, nullptr);
    groupObjectsLod2 = std::vector<std::shared_ptr<WmoGroupObject>>(mainGeom->groupsLen, nullptr);
    drawGroupWMO = std::vector<bool>(mainGeom->groupsLen, false);
    lodGroupLevelWMO = std::vector<int>(mainGeom->groupsLen, 0);

    std::string nameTemplate = m_modelName.substr(0, m_modelName.find_last_of("."));
    for(int i = 0; i < mainGeom->groupsLen; i++) {

        groupObjects[i] = std::make_shared<WmoGroupObject>(this->m_placementMatrix, m_api, mainGeom->groups[i], i);
        groupObjects[i]->setWmoApi(this);

        if (mainGeom->gfids.size() > 0) {
            groupObjects[i]->setModelFileId(mainGeom->gfids[0][i]);
            if (mainGeom->gfids.size() > 1 && i < mainGeom->gfids[1].size() && mainGeom->gfids[1][i] > 0) {
                groupObjectsLod1[i] = std::make_shared<WmoGroupObject>(this->m_placementMatrix, m_api, mainGeom->groups[i], i);
                groupObjectsLod1[i]->setWmoApi(this);
                groupObjectsLod1[i]->setModelFileId(mainGeom->gfids[1][i]);
            }
            if (mainGeom->gfids.size() > 2 && i < mainGeom->gfids[2].size() && mainGeom->gfids[2][i] > 0) {
                groupObjectsLod2[i] = std::make_shared<WmoGroupObject>(this->m_placementMatrix, m_api, mainGeom->groups[i], i);
                groupObjectsLod2[i]->setWmoApi(this);
                groupObjectsLod2[i]->setModelFileId(mainGeom->gfids[2][i]);
            }
        } else if (!useFileId) {
            std::string numStr = std::to_string(i);
            for (int j = numStr.size(); j < 3; j++) numStr = '0' + numStr;

            std::string groupFilename = nameTemplate + "_" + numStr + ".wmo";
            std::string groupFilenameLod1 = nameTemplate + "_" + numStr + "_lod1.wmo";
            std::string groupFilenameLod2 = nameTemplate + "_" + numStr + "_lod2.wmo";
            groupObjects[i]->setModelFileName(groupFilename);

            groupObjectsLod1[i] = std::make_shared<WmoGroupObject>(this->m_placementMatrix, m_api, mainGeom->groups[i], i);
            groupObjectsLod1[i]->setWmoApi(this);
            groupObjectsLod1[i]->setModelFileName(groupFilenameLod1);

            groupObjectsLod2[i] = std::make_shared<WmoGroupObject>(this->m_placementMatrix, m_api, mainGeom->groups[i], i);
            groupObjectsLod2[i]->setWmoApi(this);
            groupObjectsLod2[i]->setModelFileName(groupFilenameLod2);
        }
    }
}

void WmoObject::createWorldPortals() {

    int portalCnt = mainGeom->portalsLen;
    auto &portals = mainGeom->portals;
    auto &portalVerticles = mainGeom->portal_vertices;

    if (portalCnt <= 0) return;
    geometryPerPortal = std::vector<PortalInfo_t>(portalCnt);

    for (int j = 0; j < portalCnt; j++) {
        const SMOPortal *portalInfo = &portals[j];


        int base_index = portalInfo->base_index;
        std::vector <mathfu::vec3> portalVecs;
        for (int k = 0; k < portalInfo->index_count; k++) {
            mathfu::vec3 verticle = mathfu::vec3(
                portalVerticles[base_index + k].x,
                portalVerticles[base_index + k].y,
                portalVerticles[base_index + k].z);

            portalVecs.emplace_back() = verticle;
        }

        //Calculate center of the portal
        mathfu::vec3 center(0,0,0);
        for (int k = 0; k < portalVecs.size(); k++) {
            center += portalVecs[k];
        }
        center *= 1.0f / (float)portalVecs.size();

        //Calculate create projection and calc simplified, sorted polygon
        mathfu::vec3 lookAt = center + mathfu::vec3(portalInfo->plane.planeGeneral.normal);
        mathfu::vec3 upVector = portalVecs[0] - center;

        mathfu::mat4 viewMat = mathfu::mat4::LookAt(
                lookAt,
                center,
                upVector
//                1.0f
        );
        mathfu::mat4 viewMatInv = viewMat.Inverse();

        framebased::vector <mathfu::vec3> portalTransformed(portalVecs.size());
        for (int k = 0; k < portalVecs.size(); k++) {
            portalTransformed[k] = (viewMat * mathfu::vec4(portalVecs[k], 1.0)).xyz();
        }

        framebased::vector<mathfu::vec3> hulled = MathHelper::getHullPoints(portalTransformed);

        portalVecs.clear();
        for (int k = 0; k < hulled.size(); k++) {
            portalVecs.emplace_back() = (viewMatInv * mathfu::vec4(hulled[k], 1.0)).xyz();
        }
        geometryPerPortal[j].sortedVericles = portalVecs;

        //Calc CAAbox
        mathfu::vec3 min(99999,99999,99999), max(-99999,-99999,-99999);

        for (const auto & portalVec : portalVecs) {
            min = mathfu::vec3::Min(portalVec, min);
            max = mathfu::vec3::Max(portalVec, max);
        }

        CAaBox &aaBoxCopyTo = geometryPerPortal[j].aaBox;

        aaBoxCopyTo = CAaBox(C3Vector(min), C3Vector(max));;
    }
}

bool WmoObject::doPostLoad(const HMapSceneBufferCreate &sceneRenderer) {
    if (!m_loaded) {
        if (mainGeom != nullptr && mainGeom->getStatus() == FileStatus::FSLoaded){

            this->createMaterialCache();
            this->createNewLights();
            this->createGroupObjects();
            this->calculateAmbient();
            this->createWorldPortals();
            this->createBB(mainGeom->header->bounding_box);
            m_wmoModelChunk = sceneRenderer->createWMOWideChunk(mainGeom->groupsLen);

            if ((mainGeom->skyBoxM2FileName != nullptr && mainGeom->skyBoxM2FileNameLen > 0) || mainGeom->skyboxM2FileId != 0) {
                skyBox = m2Factory->createObject(m_api, true);
                skyBox->setLoadParams(0, {},{});

                if ( mainGeom->skyboxM2FileId != 0) {
                    skyBox->setModelFileId(mainGeom->skyboxM2FileId);
                } else {
                    skyBox->setModelFileName(&mainGeom->skyBoxM2FileName[0]);
                }
                skyBox->createPlacementMatrix(mathfu::vec3(0,0,0), 0, mathfu::vec3(1,1,1), nullptr);
                skyBox->calcWorldPosition();
            }

            m_loaded = true;
            m_loading = false;
            return true;
        } else {
            this->startLoading();
        }

        return false;
    }

    return false;
}

void WmoObject::update() {
    if (!m_loaded) return;

    if (m_placementMatChanged) {
        auto &placementMat = m_wmoModelChunk->m_placementMatrix->getObject();
        placementMat.uPlacementMat = m_placementMatrix;
        m_wmoModelChunk->m_placementMatrix->save();

        m_placementMatChanged = false;
    }
    if (m_interiorAmbientsChanged) {
        auto &groupInteriorData = m_wmoModelChunk->m_groupInteriorData->getObject();
        int interCount = (int) std::min(m_groupInteriorData.size(), (size_t) MAX_WMO_GROUPS);
        std::copy(m_groupInteriorData.data(), m_groupInteriorData.data() + interCount, groupInteriorData.interiorData);

        m_wmoModelChunk->m_groupInteriorData->save();
    }

    for (int i= 0; i < groupObjects.size(); i++) {
        if(groupObjects[i] != nullptr) {
            groupObjects[i]->update();
        }
    }
    for (int i= 0; i < groupObjectsLod1.size(); i++) {
        if(groupObjectsLod1[i] != nullptr) {
            groupObjectsLod1[i]->update();
        }
    }
    for (int i= 0; i < groupObjectsLod2.size(); i++) {
        if(groupObjectsLod2[i] != nullptr) {
            groupObjectsLod2[i]->update();
        }
    }

}
void WmoObject::uploadGeneratorBuffers() {
    if (!m_loaded) return;

//    for (int i= 0; i < groupObjects.size(); i++) {
//        if(groupObjects[i] != nullptr) {
//            groupObjects[i]->uploadGeneratorBuffers();
//        }
//    }
}

void WmoObject::collectMeshes(std::vector<HGMesh> &renderedThisFrame){
    if (!m_loaded) return;
    //Draw debug portals/Lights here?
}

void WmoObject::drawDebugLights(){
    if (!m_loaded) return;

    /*
    auto drawPointsShader = m_api->getDrawPointsShader();

    glUniformMatrix4fv(drawPointsShader->getUnf("uPlacementMat"), 1, GL_FALSE, &this->m_placementMatrix[0]);

    if (!this->m_loaded) return;

    SMOLight * lights = getLightArray();

    std::vector<float> points;

    for (int i = 0; i < mainGeom->lightsLen; i++) {
        points.push_back(lights[i].position.x);
        points.push_back(lights[i].position.y);
        points.push_back(lights[i].position.z);
    }

    GLuint bufferVBO;
    glGenBuffers(1, &bufferVBO);
    glBindBuffer( GL_ARRAY_BUFFER, bufferVBO);
    if (points.size() > 0) {
        glBufferData(GL_ARRAY_BUFFER, points.size() * 4, &points[0], GL_STATIC_DRAW);
    }

     static float colorArr[4] = {0.819607843, 0.058, 0.058, 0.3};
    glUniform3fv(drawPointsShader->getUnf("uColor"), 1, &colorArr[0]);

#ifndef WITH_GLESv2
    glEnable( GL_PROGRAM_POINT_SIZE );
#endif
    glVertexAttribPointer(+drawPoints::Attribute::aPosition, 3, GL_FLOAT, GL_FALSE, 0, 0);  // position


    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glDrawArrays(GL_POINTS, 0, points.size()/3.0f);

#ifndef WITH_GLESv2
    glDisable( GL_PROGRAM_POINT_SIZE );
#endif
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, GL_ZERO);
    glBindBuffer( GL_ARRAY_BUFFER, GL_ZERO);

    glDepthMask(GL_TRUE);

    glDeleteBuffers(1, &bufferVBO);



    for (int i= 0; i < groupObjects.size(); i++) {
        if(groupObjects[i] != nullptr) {
            groupObjects[i]->drawDebugLights();
        }
    }
 */
}

void WmoObject::setLoadingParam(const SMMapObjDef &mapObjDef) {
    createPlacementMatrix(mapObjDef);

    m_activeDoodadSets.set(mapObjDef.doodadSet);
    this->m_nameSet = mapObjDef.nameSet;
}
void WmoObject::setLoadingParam(const SMMapObjDefObj1 &mapObjDef) {
    createPlacementMatrix(mapObjDef);

    m_activeDoodadSets.set(mapObjDef.doodadSet);
    this->m_nameSet = mapObjDef.nameSet;
}

HGSamplableTexture WmoObject::getTexture(int textureId, bool isSpec) {
    if (textureId == 0) return nullptr; //Usual case

    //Non-usual case
    if (textureId < 0 || (mainGeom->textureNamesField != nullptr && textureId >= mainGeom->textureNamesFieldLen)) {
        debuglog("Non valid textureindex for WMO")
        return nullptr;
    };

    robin_hood::unordered_flat_map<int, HGSamplableTexture> &textureCache =
        !isSpec ? diffuseTextures : specularTextures;

    auto i = textureCache.find(textureId);
    if (i != textureCache.end()) {
        return i->second;
    }
    HBlpTexture texture;
    if (mainGeom->textureNamesField != nullptr) {
        std::string materialTexture(&mainGeom->textureNamesField[textureId]);
        if (materialTexture == "") return nullptr;

        if (isSpec) {
            materialTexture = materialTexture.substr(0, materialTexture.length() - 4) + "_s.blp";
        }

        texture = m_api->cacheStorage->getTextureCache()->get(materialTexture);
    } else {
        texture = m_api->cacheStorage->getTextureCache()->getFileId(textureId);
    }

    auto hgTexture = m_api->hDevice->createBlpTexture(texture, true, true);
    textureCache[textureId] = hgTexture;

    return hgTexture;
}

void WmoObject::createBB(CAaBox bbox) {
//            groupInfo = this.groupInfo;
//            bb1 = groupInfo.bb1;
//            bb2 = groupInfo.bb2;
//        } else {
//            groupInfo = this.wmoGeom.wmoGroupFile.mogp;
//            bb1 = groupInfo.BoundBoxCorner1;
//            bb2 = groupInfo.BoundBoxCorner2;
//        }
    C3Vector &bb1 = bbox.min;
    C3Vector &bb2 = bbox.max;

    mathfu::vec4 bb1vec = mathfu::vec4(bb1.x, bb1.y, bb1.z, 1);
    mathfu::vec4 bb2vec = mathfu::vec4(bb2.x, bb2.y, bb2.z, 1);

    CAaBox worldAABB = MathHelper::transformAABBWithMat4(m_placementMatrix, bb1vec, bb2vec);

    this->m_bbox = worldAABB;
}

void WmoObject::updateBB() {
    CAaBox &AABB = this->m_bbox;

    for (int j = 0; j < this->groupObjects.size(); j++) {
        std::shared_ptr<WmoGroupObject> wmoGroupObject= this->groupObjects[j];


        CAaBox groupAAbb = wmoGroupObject->getWorldAABB();

        //2. Update the world group BB
        AABB.min = mathfu::vec3_packed(mathfu::vec3(std::min(groupAAbb.min.x,AABB.min.x),
                                                         std::min(groupAAbb.min.y,AABB.min.y),
                                                         std::min(groupAAbb.min.z,AABB.min.z)));

        AABB.max = mathfu::vec3_packed(mathfu::vec3(std::max(groupAAbb.max.x,AABB.max.x),
                                                         std::max(groupAAbb.max.y,AABB.max.y),
                                                         std::max(groupAAbb.max.z,AABB.max.z)));
    }
}

CAaBox WmoObject::getAABB() {
    return this->m_bbox;
}

void WmoObject::createMaterialCache() {
    m_materialCache = decltype(m_materialCache)(mainGeom->materialsLen);
}

void WmoObject::postWmoGroupObjectLoad(int groupId, int lod) {
    //1. Create portal verticles from geometry
}

void WmoObject::checkGroupDoodads(int groupId, mathfu::vec4 &cameraVec4,
                                  std::vector<mathfu::vec4> &frustumPlane,
                                  M2ObjectListContainer &m2Candidates) {
    std::shared_ptr<WmoGroupObject> groupWmoObject = groupObjects[groupId];
    if (groupWmoObject != nullptr && groupWmoObject->getIsLoaded()) {

        for (auto &m2Object : groupWmoObject->getDoodads()) {
            if (!m2Object) continue;

            m2Candidates.addCandidate(m2Object);
        }
    }
}

bool WmoObject::startTraversingWMOGroup(
    mathfu::vec4 &cameraVec4,
    const MathHelper::FrustumCullingData &frustumDataGlobal,
    int groupId,
    int globalLevel,
    int &renderOrder,
    bool traversingFromInterior,
    FrameViewsHolder &viewsHolder
) {
    ZoneScoped;
    if (!m_loaded)
        return false;

    uint32_t portalCount = (uint32_t) std::max(0, this->mainGeom->portalsLen);

    if (portalCount == 0) {
        if (groupId > -1 && groupId < mainGeom->groupsLen &&
            mainGeom->groups[groupId].flags.INTERIOR && !mainGeom->groups[groupId].flags.EXTERIOR_CULL)
        {
            auto nextGroupObject = groupObjects[groupId];

            auto interiorView = viewsHolder.createInterior(frustumDataGlobal);
            interiorView->ownerGroupWMO = groupObjects[groupId];
            interiorView->wmoGroupArray.addToDraw(nextGroupObject);
            interiorView->wmoGroupArray.addToCheckM2(nextGroupObject);

            return true;
        } else {
            auto exteriorView = viewsHolder.getOrCreateExterior(frustumDataGlobal);
            bool result = false;
            for (int i = 0; i< mainGeom->groupsLen; i++) {
                if ((mainGeom->groups[i].flags.EXTERIOR) > 0 || (mainGeom->groups[i].flags.EXTERIOR_CULL) > 0 || !m_api->getConfig()->usePortalCulling) { //exterior
                    if (this->groupObjects[i] != nullptr) {
                        bool drawDoodads, drawGroup;
                        this->groupObjects[i]->checkGroupFrustum(drawDoodads, drawGroup, cameraVec4, frustumDataGlobal);
                        if (drawDoodads) {
                            exteriorView->wmoGroupArray.addToCheckM2(this->groupObjects[i]);
                        }
                        if (drawGroup) {
                            exteriorView->wmoGroupArray.addToDraw(this->groupObjects[i]);
                        }
                        result |= drawGroup;
                    }
                }
            }
            return result;
        }
    }
    framebased::vector<HInteriorView> ivPerWMOGroup = framebased::vector<HInteriorView>(mainGeom->groupsLen);

    framebased::vector<bool> transverseVisitedPortals = framebased::vector<bool>(portalCount, false);

    //CurrentVisibleM2 and visibleWmo is array of global m2 objects, that are visible after frustum
    mathfu::vec4 cameraLocal = this->m_placementInvertMatrix * cameraVec4;

    mathfu::mat4 transposeModelMat = this->m_placementMatrix.Transpose();
    mathfu::mat4 transposeInverseModelMat = transposeModelMat.Inverse();

    //For interior cull
    mathfu::mat4 MVPMat = frustumDataGlobal.perspectiveMat*frustumDataGlobal.viewMat*this->m_placementMatrix;
    mathfu::mat4 MVPMatInv = MVPMat.Inverse();

    framebased::vector<mathfu::vec3> frustumPointsLocal = MathHelper::calculateFrustumPointsFromMat(MVPMat);
    framebased::vector<mathfu::vec4> frustumPlanesLocal = MathHelper::getFrustumClipsFromMatrix(MVPMat);
//    mathfu::vec4 headOfPyramidLocal = mathfu::vec4(MathHelper::getIntersection(
//        frustumPointsLocal[4], frustumPointsLocal[7],
//        frustumPointsLocal[3], frustumPointsLocal[0]
//    ), 1.0);

    auto globalPlane = frustumDataGlobal.frustums[0].planes[frustumDataGlobal.frustums[0].planes.size() - 2];
    auto altFarPlane = this->m_placementMatrix.Transpose() * globalPlane;
    PortalTraverseTempData traverseTempData = {
        viewsHolder,
        viewsHolder.getExterior() != nullptr,
        frustumPlanesLocal[frustumPlanesLocal.size() - 1], //farPlane is always last one,
        ivPerWMOGroup,
        cameraVec4,
        cameraLocal,
        transposeInverseModelMat,
        MVPMat,
        MVPMatInv,
        transverseVisitedPortals
    };

    if (traversingFromInterior && m_api->getConfig()->usePortalCulling) {
        traverseTempData.atLeastOneGroupIsDrawn = true;

        std::shared_ptr<WmoGroupObject> nextGroupObject = groupObjects[groupId];
        if (nextGroupObject->getIsLoaded() && nextGroupObject->getWmoGroupGeom()->mogp->flags2.isSplitGroupChild) {
            groupId = nextGroupObject->getWmoGroupGeom()->mogp->parentSplitOrFirstChildGroupIndex;
            nextGroupObject = groupObjects[groupId];
        }

        auto interiorView = ivPerWMOGroup[groupId];
        //5.1 The portal is into interior wmo group. So go on.
        if (interiorView == nullptr) {
            interiorView = viewsHolder.createInterior(frustumDataGlobal);
            ivPerWMOGroup[groupId] = interiorView;
            interiorView->ownerGroupWMO = groupObjects[groupId];
            interiorView->wmoGroupArray.addToDraw(nextGroupObject);
            interiorView->wmoGroupArray.addToCheckM2(nextGroupObject);
        }

        if (nextGroupObject->getIsLoaded() && nextGroupObject->getWmoGroupGeom()->mogp->flags2.isSplitGroupParent) {
            this->addSplitChildWMOsToView(*interiorView, groupId);
        }

        if (globalLevel+1 >= interiorView->level) {
            interiorView->level = globalLevel + 1;
        } else {
            assert("BVH is not working. Something is wrong!");
        }

        this->traverseGroupWmo(
            groupId,
            traversingFromInterior,
            traverseTempData,
            frustumPlanesLocal,
            globalLevel,
            0);
    } else {
        auto exteriorView = viewsHolder.getOrCreateExterior(frustumDataGlobal);

        if (globalLevel+1 >= exteriorView->level) {
            exteriorView->level = globalLevel + 1;
        } else {
            assert("BVH is not working. Something is wrong!");
        }

        for (int i = 0; i< mainGeom->groupsLen; i++) {
            if ((mainGeom->groups[i].flags.EXTERIOR) > 0 || !m_api->getConfig()->usePortalCulling) { //exterior
                if (this->groupObjects[i] != nullptr) {
                    bool drawDoodads, drawGroup;
                    this->groupObjects[i]->checkGroupFrustum(drawDoodads, drawGroup, cameraVec4, frustumDataGlobal);
                    if (drawDoodads) {
                        exteriorView->wmoGroupArray.addToCheckM2(this->groupObjects[i]);
                    }
                    if (drawGroup) {
                        exteriorView->wmoGroupArray.addToDraw(this->groupObjects[i]);
                        traverseTempData.atLeastOneGroupIsDrawn = true;
                        if (m_api->getConfig()->usePortalCulling && portalCount > 0) {
                            this->traverseGroupWmo(
                                i,
                                false,
                                traverseTempData,
                                frustumPlanesLocal,
                                globalLevel,
                                0);
                        }
                    }
                }
            }
        }
    }

    //Add all ALWAYSRENDER to Exterior
    for (int i = 0; i< mainGeom->groupsLen; i++) {
        if ((mainGeom->groups[i].flags.ALWAYSDRAW) > 0) { //exterior
            auto exteriorView = viewsHolder.getOrCreateExterior(frustumDataGlobal);
            exteriorView->wmoGroupArray.addToDraw(this->groupObjects[i]);
        }

        if (m_api->getConfig()->renderAntiPortals) {
            auto exteriorView = viewsHolder.getOrCreateExterior(frustumDataGlobal);
            if (!this->groupObjects[i]->getIsLoaded()) {
                exteriorView->wmoGroupArray.addToLoad(this->groupObjects[i]);
            } else {
                if ((mainGeom->groups[i].flags.ANTIPORTAL) > 0) { //ANTIPORTAL
                    if (this->groupObjects[i]->getIsLoaded()) {
                        exteriorView->worldAntiPortalVertices.emplace_back() =
                            createAntiPortal(this->groupObjects[i]->getWmoGroupGeom(), m_placementMatrix);
                    }
                }
            }
        }
    }

    //Process results
    ivPerWMOGroup.erase(std::remove(ivPerWMOGroup.begin(), ivPerWMOGroup.end(), nullptr), ivPerWMOGroup.end());
    std::sort(ivPerWMOGroup.begin(), ivPerWMOGroup.end(), [](const HInteriorView &a, const HInteriorView &b) -> bool {
        if (a->level != b->level) {
            return a->level < b->level;
        }
        return false;
    });

    for (auto &createdInteriorView : ivPerWMOGroup) {
        createdInteriorView->renderOrder = renderOrder++;
    }

    bool result = ivPerWMOGroup.size() > 0;
    {
        auto exterior = viewsHolder.getExterior();

        if (exterior != nullptr) {
            exterior->renderOrder = renderOrder++;
            result = true;
        }
    }

    //M2s will be collected later from separate function call
    return traverseTempData.atLeastOneGroupIsDrawn;
}
void WmoObject::addSplitChildWMOsToView(InteriorView &interiorView, int groupId) {
    if (!groupObjects[groupId]->getIsLoaded())
        return;

    auto &parentMogp = groupObjects[groupId]->getWmoGroupGeom()->mogp;
    if (!parentMogp->flags2.isSplitGroupParent)
        return;

    int nextChildGroupIndex = parentMogp->parentSplitOrFirstChildGroupIndex;
    while (nextChildGroupIndex != -1) {
        auto &groupWmo = groupObjects[nextChildGroupIndex];
        if (!groupWmo->getIsLoaded()) {
            interiorView.wmoGroupArray.addToLoad(groupWmo);
            return;
        }

        auto &mogp = groupWmo->getWmoGroupGeom()->mogp;
        if (!mogp->flags2.isSplitGroupChild)
            break;

        interiorView.wmoGroupArray.addToDraw(groupWmo);
        interiorView.wmoGroupArray.addToCheckM2(groupWmo);

        nextChildGroupIndex = mogp->nextSplitGroupChildIndex;
    }

}


static const float dotepsilon = pow(1.5f, 2.0f);
void WmoObject::traverseGroupWmo(
    int groupId,
    bool traversingStartedFromInterior,
    PortalTraverseTempData &traverseTempData,
    framebased::vector<mathfu::vec4> &localFrustumPlanes,
    int globalLevel,
    int localLevel
) {

    if (localLevel > 8) return;

    if (groupObjects[groupId] == nullptr || !groupObjects[groupId]->getIsLoaded()) {
        //The group has not been loaded yet
        return;
    }

    //2. Loop through portals of current group

    int moprIndex = groupObjects[groupId]->getWmoGroupGeom()->mogp->moprIndex;
    int numItems = groupObjects[groupId]->getWmoGroupGeom()->mogp->moprCount;

    if (groupObjects[groupId]->getWmoGroupGeom()->mogp->flags.showSkyBox) {
        if (groupObjects[groupId]->getWmoGroupGeom()->mogp->flags.INTERIOR > 0 || !m_api->getConfig()->usePortalCulling) {
            if (traversingStartedFromInterior && skyBox != nullptr) {
                traverseTempData.viewsHolder.getSkybox()->m2List.addToDraw(skyBox);
            }
        } else {
            //TODO: WHAT ????
            //Example: main wmo: 850548, group WMO 901743
        }
    }

    for (int j = moprIndex; j < moprIndex+numItems; j++) {
        const SMOPortalRef * relation = &mainGeom->portalReferences[j];
        const SMOPortal * portalInfo = &mainGeom->portals[relation->portal_index];

        int nextGroup = relation->group_index;
        const C4Plane &plane = portalInfo->plane;

        //Skip portals we already visited
        if (traverseTempData.transverseVisitedPortals[relation->portal_index]) continue;

        //Local coordinanes plane DOT local camera
        const mathfu::vec4 planeV4 = mathfu::vec4(plane.planeVector);
        float dotResult = mathfu::vec4::DotProduct(planeV4, traverseTempData.cameraLocal);
        //dotResult = dotResult + relation.side * 0.01;
        bool isInsidePortalThis = (relation->side < 0) ? (dotResult <= 0) : (dotResult >= 0);

        //This condition checks if camera is very close to the portal. In this case the math doesnt work very properly
        //So I need to make this hack exactly for this case.z

        bool hackCondition = (fabs(dotResult) > dotepsilon);

        /* Test code
        auto crossProd = mathfu::vec3::CrossProduct(
            geometryPerPortal[relation->portal_index].sortedVericles[0] - geometryPerPortal[relation->portal_index].sortedVericles[1],
            geometryPerPortal[relation->portal_index].sortedVericles[0] - geometryPerPortal[relation->portal_index].sortedVericles[2]
        );
        crossProd = crossProd.Normalized();
        float distantce = mathfu::vec3::DotProduct(crossProd,geometryPerPortal[relation->portal_index].sortedVericles[0]);
        mathfu::vec4 manualEquation = mathfu::vec4(crossProd.x, crossProd.y, crossProd.z, -distantce);
        */
        //Hacks to test!
        /*
        if (!isInsidePortalThis)
        {
            std::vector<mathfu::vec3> worldSpacePortalVertices;
            std::vector<mathfu::vec3> portalVerticesVec;
            std::transform(
                geometryPerPortal[relation->portal_index].sortedVericles.begin(),
                geometryPerPortal[relation->portal_index].sortedVericles.end(),
                std::back_inserter(portalVerticesVec),
                [](mathfu::vec3 &d) -> mathfu::vec3 { return d;}
            );
            std::transform(portalVerticesVec.begin(), portalVerticesVec.end(),
                           std::back_inserter(worldSpacePortalVertices),
                           [&](mathfu::vec3 &p) -> mathfu::vec3 {
                               return (this->m_placementMatrix * mathfu::vec4(p, 1.0f)).xyz();
                           }
            );
            HInteriorView &interiorView = traverseTempData.ivPerWMOGroup[groupId];
            if (interiorView == nullptr) {
                interiorView = traverseTempData.viewsHolder.createInterior({});
                traverseTempData.ivPerWMOGroup[nextGroup] = interiorView;

                interiorView->ownerGroupWMO = groupObjects[nextGroup];
                interiorView->drawnWmos.insert(groupObjects[nextGroup]);
                interiorView->wmosForM2.insert(groupObjects[nextGroup]);
                interiorView->portalIndexes.push_back(relation->portal_index);
            }
            interiorView->worldPortalVertices.push_back(worldSpacePortalVertices);
        }
         */

        if (!isInsidePortalThis && hackCondition) continue;

        //2.1 If portal has less than 4 vertices - skip it(invalid?)
        if (portalInfo->index_count < 4) continue;

        //2.2 Check if Portal BB made from portal vertexes intersects frustum
        framebased::vector<mathfu::vec3> portalVerticesVec;
        std::transform(
            geometryPerPortal[relation->portal_index].sortedVericles.begin(),
            geometryPerPortal[relation->portal_index].sortedVericles.end(),
            std::back_inserter(portalVerticesVec),
            [](mathfu::vec3 &d) -> mathfu::vec3 { return d;}
            );

        bool visible = MathHelper::planeCull(portalVerticesVec, localFrustumPlanes);

        if (!visible && hackCondition) continue;

        traverseTempData.transverseVisitedPortals[relation->portal_index] = true;

        int lastFrustumPlanesLen = localFrustumPlanes.size();

        //3. Construct frustum planes for this portal
        framebased::vector<mathfu::vec4> thisPortalPlanes;
        thisPortalPlanes.reserve(lastFrustumPlanesLen);

        if (hackCondition) {
            //Transform portal vertexes into clip space (this code should allow to use this logic with both Perspective and Ortho)
            framebased::vector<mathfu::vec4> portalVerticesClip(portalVerticesVec.size());
            framebased::vector<mathfu::vec4> portalVerticesClipNearPlane(portalVerticesVec.size());

            for (int i = 0; i < portalVerticesVec.size(); i++) {
                portalVerticesClip[i] = traverseTempData.MVPMat * mathfu::vec4(portalVerticesVec[i], 1.0f);
                portalVerticesClip[i] /= portalVerticesClip[i].w;
            }



            for (int i = 0; i < portalVerticesVec.size(); i++) {
                //Project Portal vertex to near plane in clip space
                portalVerticesClipNearPlane[i] = portalVerticesClip[i];
                portalVerticesClipNearPlane[i].z = -1;
                //Transform back to local space
                portalVerticesClipNearPlane[i] = traverseTempData.MVPMatInv * portalVerticesClipNearPlane[i];
                portalVerticesClipNearPlane[i] /= portalVerticesClipNearPlane[i].w;
            }

            //This condition works, bcuz earlier it's verified we are inside the portal using the
            //dot product AND `relation->side`
            bool flip = (relation->side > 0);
            
            int vertexCnt = portalVerticesVec.size();
            for (int i = 0; i < vertexCnt; ++i) {
                int i2 = (i + 1) % vertexCnt;

                mathfu::vec4 n = MathHelper::createPlaneFromEyeAndVertexes(portalVerticesClipNearPlane[i].xyz(),
                                                                           portalVerticesVec[i],
                                                                           portalVerticesVec[i2]);

                if (flip) {
                    n *= -1.0f;
                }

                thisPortalPlanes.emplace_back() = n;
            }
            //The portalPlanes do not have far and near plane. So we need to add them
            //Near plane is this portal's plane, far plane is a global one
            auto nearPlane = mathfu::vec4(portalInfo->plane.planeVector);
            if (flip)
                nearPlane *= -1.0f;

            auto &farPlane = traverseTempData.farPlane;
            thisPortalPlanes.emplace_back() = nearPlane;
            thisPortalPlanes.emplace_back() = farPlane;
        } else {
            // If camera is too close - just use usual frustums
            thisPortalPlanes = localFrustumPlanes;
        }

        //Transform local planes into world planes to use with frustum culling of M2Objects
        MathHelper::PlanesUndPoints worldSpaceFrustum;
        worldSpaceFrustum.planes = framebased::vector<mathfu::vec4>(thisPortalPlanes.size());
        worldSpaceFrustum.points = framebased::vector<mathfu::vec3>();
        worldSpaceFrustum.points.reserve(thisPortalPlanes.size());

        for (int x = 0; x < thisPortalPlanes.size(); x++) {
            worldSpaceFrustum.planes[x] = traverseTempData.transposeInverseModelMat * thisPortalPlanes[x];
        }

        {
            worldSpaceFrustum.points = MathHelper::getIntersectionPointsFromPlanes(worldSpaceFrustum.planes);
            //worldSpaceFrustum.hullLines = MathHelper::getHullLines(worldSpaceFrustum.points);
        }

        std::vector<mathfu::vec3> worldSpacePortalVertices;
        std::transform(portalVerticesVec.begin(), portalVerticesVec.end(),
                       std::back_inserter(worldSpacePortalVertices),
                       [&](mathfu::vec3 &p) -> mathfu::vec3 {
                           return (this->m_placementMatrix * mathfu::vec4(p, 1.0f)).xyz();
                       }
        );

        //5. Traverse next
        std::shared_ptr<WmoGroupObject> &nextGroupObject = groupObjects[nextGroup];
        const SMOGroupInfo &nextGroupInfo = mainGeom->groups[nextGroup];
        if ((nextGroupInfo.flags.EXTERIOR) == 0) {
            auto &interiorView = traverseTempData.ivPerWMOGroup[nextGroup];
            //5.1 The portal is into interior wmo group. So go on.
            if (interiorView == nullptr) {
                interiorView = traverseTempData.viewsHolder.createInterior({});
                traverseTempData.ivPerWMOGroup[nextGroup] = interiorView;

                interiorView->ownerGroupWMO = nextGroupObject;
                interiorView->wmoGroupArray.addToDraw(nextGroupObject);
                interiorView->wmoGroupArray.addToCheckM2(nextGroupObject);
                interiorView->portalIndexes.emplace_back() = relation->portal_index;
            }

            interiorView->worldPortalVertices.emplace_back() = worldSpacePortalVertices;
            interiorView->frustumData.frustums.emplace_back() = worldSpaceFrustum;
            if (globalLevel+1 >= interiorView->level) {
                interiorView->level = globalLevel + 1;
            } else {
                assert("BVH is not working. Something is wrong!");
            }

            if (nextGroupObject->getIsLoaded() && nextGroupObject->getWmoGroupGeom()->mogp->flags2.isSplitGroupParent) {
                this->addSplitChildWMOsToView(*interiorView, nextGroup);
            }

            traverseGroupWmo(
                nextGroup,
                traversingStartedFromInterior,
                traverseTempData,
                thisPortalPlanes,
                globalLevel + 1,
                localLevel + 1
            );

        } else if (((nextGroupInfo.flags.EXTERIOR) > 0) && traversingStartedFromInterior) {
            //5.2 The portal is from interior into exterior wmo group.
            //Makes sense to try to create or get only if exterior was not already created before traversing this entire WMO
            if (!traverseTempData.exteriorWasCreatedBeforeTraversing) {
                auto exteriorView = traverseTempData.viewsHolder.getOrCreateExterior({});
                exteriorView->level = globalLevel + 1;
                exteriorView->worldPortalVertices.emplace_back() = worldSpacePortalVertices;
                exteriorView->frustumData.frustums.emplace_back() = worldSpaceFrustum;
            }
        }
    }
}

bool WmoObject::isGroupWmoInterior(int groupId) {
    const SMOGroupInfo *groupInfo = &this->mainGeom->groups[groupId];
    bool result = ((groupInfo->flags.EXTERIOR) == 0);
    return result;
}

bool WmoObject::isGroupWmoExteriorLit(int groupId) {
    const SMOGroupInfo *groupInfo = &this->mainGeom->groups[groupId];
    bool result = ((groupInfo->flags.EXTERIOR_LIT) == 1) || ((groupInfo->flags.EXTERIOR) == 1) || ((groupInfo->flags.INTERIOR) == 0);
    return result;
}

bool WmoObject::isGroupWmoExtSkybox(int groupId) {
    const SMOGroupInfo *groupInfo = &this->mainGeom->groups[groupId];
    bool result = ((groupInfo->flags.SHOW_EXTERIOR_SKYBOX) == 1);
    return result;
}

bool WmoObject::getGroupWmoThatCameraIsInside (mathfu::vec4 cameraVec4, WmoGroupResult &groupResult, float &bottomBorder) {
    if (this->groupObjects.size() ==0) return false;

    //Transform camera into local coordinates
    mathfu::vec4 cameraLocal = this->m_placementInvertMatrix * cameraVec4 ;
    float localBorder = (this->m_placementInvertMatrix * mathfu::vec4(0,0,bottomBorder, 1.0f)).z;

    //Check if camera inside wmo
    const auto &mainGeomBB = this->mainGeom->header->bounding_box;
    bool isInsideWMOBB = (
        cameraLocal[0] > mainGeomBB.min.x && cameraLocal[0] < mainGeomBB.max.x &&
        cameraLocal[1] > mainGeomBB.min.y && cameraLocal[1] < mainGeomBB.max.y &&
        cameraLocal[2] > mainGeomBB.min.z && cameraLocal[2] < mainGeomBB.max.z
    );
    if (!isInsideWMOBB) return false;

    //Loop
    int wmoGroupsInside = 0;
    int interiorGroups = 0;
    int lastWmoGroupInside = -1;
    std::vector<WmoGroupResult> candidateGroups;

    for (int i = 0; i < this->groupObjects.size(); i++) {
        if (this->groupObjects[i] == nullptr) continue;
        this->groupObjects[i]->checkIfInsideGroup(
                cameraVec4,
                cameraLocal,
                this->mainGeom->portal_vertices,
                this->mainGeom->portals,
                this->mainGeom->portalReferences,
                candidateGroups);
    }

    //6. Iterate through result group list and find the one with maximal bottom z coordinate for object position
    float minDist = 999999;
    bool result = false;
    for (int i = 0; i < candidateGroups.size(); i++) {
        WmoGroupResult *candidate = &candidateGroups[i];
        const SMOGroupInfo *groupInfo = &this->mainGeom->groups[candidate->groupIndex];
        /*if ((candidate.topBottom.bottomZ < 99999) && (candidate.topBottom.topZ > -99999)){
            if ((cameraLocal[2] < candidateGroups[i].topBottom.bottomZ) || (cameraLocal[2] > candidateGroups[i].topBottom.topZ))
                continue
        } */
        if (candidate->topBottom.min < 99999 && candidate->topBottom.min > localBorder) {
            float dist = cameraLocal[2] - candidate->topBottom.min;
            if (dist > 0 && dist < minDist) {
                result = true;
                minDist = dist;
                groupResult = *candidate;

                localBorder = candidate->topBottom.min;
            }
        }
    }

    bottomBorder = (this->m_placementMatrix * mathfu::vec4(0,0,localBorder, 1.0f)).z;

    return result;
}

std::string WmoObject::getModelFileName() {
    return m_modelName;
}
void WmoObject::setModelFileName(std::string modelName) {
    m_modelName = modelName;
}
int WmoObject::getModelFileId() {
   return m_modelFileId;
}
void WmoObject::setModelFileId(int fileId) {
    useFileId = true;
    m_modelFileId = fileId;
}

void WmoObject::fillLodGroup(mathfu::vec3 &cameraLocal) {
    for(int i = 0; i < mainGeom->groupsLen; i++) {
        if (drawGroupWMO[i]) {
            float distance = MathHelper::distanceFromAABBToPoint(groupObjects[i]->getLocalAABB(), cameraLocal);
            if (distance > 800) {
                lodGroupLevelWMO[i] = 2;
            } else if (distance > 500) {
                lodGroupLevelWMO[i] = 1;
            } else {
                lodGroupLevelWMO[i] = 0;
            }

            lodGroupLevelWMO[i] = std::min(lodGroupLevelWMO[i], getWmoHeader()->numLod-1);
        } else {
            lodGroupLevelWMO[i] = 0;
        }
    }

}
float distance(C4Plane &plane, C3Vector vertex) {
    return (float) sqrt(mathfu::vec4::DotProduct(mathfu::vec4(plane.planeVector), mathfu::vec4(vertex.x, vertex.y, vertex.z, 1.0)));
}


void attenuateTransVerts(HWmoMainGeom &mainGeom, WmoGroupGeom& wmoGroupGeom) {

    if (!wmoGroupGeom.mogp->transBatchCount)
    {
        return;
    }

//    for ( std::size_t vertex_index (0); vertex_index < wmoGroupGeom.batches[wmoGroupGeom.mogp->transBatchCount-1].last_vertex; ++vertex_index) {
//        float opacity_accum (0.0);
//
//        for ( std::size_t portal_ref_index (wmoGroupGeom.mogp->moprIndex);
//              portal_ref_index < (wmoGroupGeom.mogp->moprIndex + wmoGroupGeom.mogp->moprCount);
//              ++portal_ref_index)
//        {
//            SMOPortalRef const& portalRef (mainGeom->portalReferences[portal_ref_index]);
//            SMOPortal const& portal (mainGeom->portals[portalRef.portal_index]);
//            C3Vector const& vertex (wmoGroupGeom.verticles[vertex_index]);
//
//            float const portal_to_vertex (distance(portal.plane, vertex));
//
//            C3Vector vertex_to_use (vertex);
//
//            if (portal_to_vertex > 0.001 || portal_to_vertex < -0.001)
//            {
//                C3Ray ray ( C3Ray::FromStartEnd
//                                    ( vertex
//                                            , vertex
//                                              + (portal_to_vertex > 0 ? -1 : 1) * portal.plane.planeGeneral.normal
//                                            , 0
//                                    )
//                );
//                NTempest::Intersect
//                        (ray, &portal.plane, 0LL, &vertex_to_use, 0.0099999998);
//            }
//
//            float distance_to_use;
//
//            if ( NTempest::Intersect ( vertex_to_use
//                    , &mainGeom->portal_vertices[portal.base_index]
//                    , portal.index_count
//                    , C3Vector::MajorAxis (portal.plane.normal)
//            )
//                    )
//            {
//                distance_to_use = portalRef.side * distance (portal.plane, vertex);
//            }
//            else
//            {
//                distance_to_use = NTempest::DistanceFromPolygonEdge
//                        (vertex, &mainGeom->portal_vertices[portal.base_index], portal.index_count);
//            }
//
//            if (mainGeom->groups[portalRef.group_index].flags.EXTERIOR ||
//                mainGeom->groups[portalRef.group_index].flags.EXTERIOR_LIT)
//            {
//                float v25 (distance_to_use >= 0.0 ? distance_to_use / 6.0f : 0.0f);
//                if ((1.0 - v25) > 0.001)
//                {
//                    opacity_accum += 1.0 - v25;
//                }
//            }
//            else if (distance_to_use > -1.0)
//            {
//                opacity_accum = 0.0;
//                if (distance_to_use < 1.0)
//                {
//                    break;
//                }
//            }
//        }
//
//        float const opacity ( opacity_accum > 0.001
//                              ? std::min (1.0f, opacity_accum)
//                              : 0.0f
//        );
//
//        //! \note all assignments asserted to be > -0.5 && < 255.5f
//        CArgb& color (wmoGroupGeom.colorArray[vertex_index]);
//        color.r = (unsigned char) (((127.0f - color.r) * opacity) + color.r);
//        color.g = (unsigned char) (((127.0f - color.g) * opacity) + color.g);
//        color.b = (unsigned char) (((127.0f - color.b) * opacity) + color.b);
//        color.a = opacity * 255.0;
//    }
}

std::function<void(WmoGroupGeom &wmoGroupGeom)> WmoObject::getAttenFunction() {
    HWmoMainGeom &mainGeom = this->mainGeom;
    return [&mainGeom](  WmoGroupGeom &wmoGroupGeom ) -> void {
        attenuateTransVerts(mainGeom, wmoGroupGeom);
    } ;
}

void WmoObject::checkFog(const mathfu::vec3 &cameraPos, std::vector<SMOFog_Data> &fogResults) {
    mathfu::vec3 cameraLocal = (m_placementInvertMatrix * mathfu::vec4(cameraPos, 1.0)).xyz();
    for (int i = mainGeom->fogsLen-1; i >= 0; i--) {
        const SMOFog &fogRecord = mainGeom->fogs[i];
        mathfu::vec3 fogPosVec = mathfu::vec3(fogRecord.pos);

        float distanceToFog = (fogPosVec - cameraLocal).Length();
        if ((distanceToFog < fogRecord.larger_radius) || fogRecord.flag_infinite_radius) {
            fogResults.push_back(fogRecord.fog);
        }
    }
}

bool WmoObject::hasPortals() {
    return mainGeom->header->nPortals != 0;
}

SMOHeader *WmoObject::getWmoHeader() {
    return mainGeom->header;
}

PointerChecker<SMOLight> &WmoObject::getLightArray() {
    return mainGeom->lights;
}

PointerChecker<SMOMaterial> &WmoObject::getMaterials() {
    return mainGeom->materials;
}

std::shared_ptr<IWMOMaterial> WmoObject::getMaterialInstance(int materialIndex, const HMapSceneBufferCreate &sceneRenderer) {
    assert(materialIndex < m_materialCache.size());

    auto materialInstance = m_materialCache[materialIndex];
    if (materialInstance != nullptr)
        return materialInstance;

    //Otherwise create goddamit material

    const SMOMaterial &material = getMaterials()[materialIndex];
    assert(material.shader < MAX_WMO_SHADERS && material.shader >= 0);
    auto shaderId = material.shader;
    if (shaderId >= MAX_WMO_SHADERS) {
        shaderId = 0;
    }

    int pixelShader = wmoMaterialShader[shaderId].pixelShader;
    int vertexShader = wmoMaterialShader[shaderId].vertexShader;

    auto blendMode = material.blendMode;

    PipelineTemplate pipelineTemplate;
    pipelineTemplate.element = DrawElementMode::TRIANGLES;
    pipelineTemplate.depthWrite = blendMode <= 1;
    pipelineTemplate.depthCulling = true;
    pipelineTemplate.backFaceCulling = !(material.flags.F_UNCULLED);

    pipelineTemplate.blendMode = static_cast<EGxBlendEnum>(blendMode);

    pipelineTemplate.stencilTestEnable = false;
    pipelineTemplate.stencilWrite = true;
    pipelineTemplate.stencilWriteVal = ObjStencilValues::WMO_STENCIL_VAL;

    bool isSecondTextSpec = material.shader == 8;

    HGSamplableTexture texture1 = getTexture(material.diffuseNameIndex, false);
    HGSamplableTexture texture2 = getTexture(material.envNameIndex, isSecondTextSpec);
    HGSamplableTexture texture3 = getTexture(material.texture_2, false);

    WMOMaterialTemplate materialTemplate;

    materialTemplate.textures[0] = texture1;
    materialTemplate.textures[1] = texture2;
    materialTemplate.textures[2] = texture3;

    if (pixelShader == (int)WmoPixelShader::MapObjParallax) {
        materialTemplate.textures[3] = getTexture(material.color_2, false);
        materialTemplate.textures[4] = getTexture(material.flags_2, false);
        materialTemplate.textures[5] = getTexture(material.runTimeData[0], false);
    } else if (pixelShader == (int)WmoPixelShader::MapObjDFShader) {
        materialTemplate.textures[3] = getTexture(material.color_2, false);
        materialTemplate.textures[4] = getTexture(material.flags_2, false);
        materialTemplate.textures[5] = getTexture(material.runTimeData[0], false);
        materialTemplate.textures[6] = getTexture(material.runTimeData[1], false);
        materialTemplate.textures[7] = getTexture(material.runTimeData[2], false);
        materialTemplate.textures[8] = getTexture(material.runTimeData[3], false);
    }

    materialInstance = sceneRenderer->createWMOMaterial(m_wmoModelChunk, pipelineTemplate, materialTemplate);
    m_materialCache[materialIndex] = materialInstance;

    C4Vector matUVAnim = C4Vector(mathfu::vec4(0.0f,0.0f,0.0f,0.0f));
    if (mainGeom->materialUVSpeedLen > 0 && (materialIndex < mainGeom->materialUVSpeedLen)) {
        matUVAnim = mainGeom->materialUVSpeed[materialIndex];
    }
    {
        float alphaTest = (blendMode > 0) ? 0.00392157f : -1.0f;

        //Update VS block
        auto &blockVS = materialInstance->m_materialVS->getObject();
        blockVS.UseLitColor = (material.flags.F_UNLIT > 0) ? 0 : 1;
        blockVS.VertexShader = vertexShader;
        blockVS.translationSpeedXY = matUVAnim;
        materialInstance->m_materialVS->save();

        //Update PS block
        auto &blockPS = materialInstance->m_materialPS->getObject();
        blockPS.UseLitColor = (material.flags.F_UNLIT > 0) ? 0 : 1;
        blockPS.EnableAlpha = (blendMode > 0) ? 1 : 0;
        blockPS.PixelShader = pixelShader;
        blockPS.BlendMode = blendMode;
        blockPS.uFogColor_AlphaTest = mathfu::vec4_packed(
            mathfu::vec4(0,0,0, alphaTest));
        materialInstance->m_materialPS->save();
    }

    return materialInstance;
};

std::shared_ptr<M2Object> WmoObject::getSkyBoxForGroup(int groupNum) {
    if (!m_loaded) return nullptr;
    if (groupNum < 0 || groupNum >= this->groupObjects.size()) return nullptr;
    if (!this->groupObjects[groupNum]->getIsLoaded()) return nullptr;
    if (!this->groupObjects[groupNum]->getWmoGroupGeom()->mogp->flags.showSkyBox) return nullptr;

    return skyBox;
}

WmoObject::~WmoObject() {
}

int WmoObject::getWmoGroupId(int groupNum) {
    if (!m_loaded) return 0;
    if (!groupObjects[groupNum]->getIsLoaded()) return 0;

    return groupObjects[groupNum]->getWmoGroupGeom()->mogp->wmoGroupID;
}

std::array<mathfu::vec3,3> WmoObject::getAmbientColors() {
    return m_ambientColors;
}

void WmoObject::createNewLights() {
    m_newLights.resize(mainGeom->newLightsLen);
    for (int i = 0; i < mainGeom->newLightsLen; i++) {
        auto &newLightRec = mainGeom->newLights[i];
        if (!m_activeDoodadSets[newLightRec.doodadSet]) {
            m_newLights[i] = nullptr;
            continue;
        }
        m_newLights[i] = std::make_shared<CWmoNewLight>(m_placementMatrix, mainGeom->newLights[i]);
    }
}
void WmoObject::calculateAmbient() {
    mathfu::vec3 ambientColor;
    mathfu::vec3 horizontAmbientColor;
    mathfu::vec3 groundAmbientColor;

    if (mainGeom->mavgsLen > 0) {
        //Take ambient from MAVG
        int recordIndex = 0;
        for (int i = 0; i < mainGeom->mavgsLen; i++) {
            if (m_activeDoodadSets[mainGeom->mavgs[i].doodadSetID]) {
                recordIndex = i;
                break;
            }
        }
        auto &record = mainGeom->mavgs[recordIndex];
        if ((record.flags & 1) != 0) {
            ambientColor = ImVectorToVec4(record.color1).xyz();
            horizontAmbientColor = ImVectorToVec4(record.color2).xyz();
            groundAmbientColor = ImVectorToVec4(record.color3).xyz();
        } else {
            auto amb = ImVectorToVec4(record.color1).xyz();
            ambientColor = amb;
            horizontAmbientColor = amb;
            groundAmbientColor = amb;
        }
    } else if (mainGeom->mavdsLen > 0) {
        //Take ambient from MAVD
        auto const &mavds = mainGeom->mavds;
        if ((mavds->flags & 1) != 0) {
            ambientColor = ImVectorToVec4(mavds->color1).xyz();
            horizontAmbientColor = ImVectorToVec4(mavds->color2).xyz();
            groundAmbientColor = ImVectorToVec4(mavds->color3).xyz();
        } else {
            auto amb = ImVectorToVec4(mavds->color1).xyz();
            ambientColor = amb;
            horizontAmbientColor = amb;
            groundAmbientColor = amb;
        }
    } else if (mainGeom->header) {
        auto amb = ImVectorToVec4(mainGeom->header->ambColor).xyz();
        ambientColor = amb;
        horizontAmbientColor = amb;
        groundAmbientColor = amb;
    }

    m_ambientColors = {ambientColor, horizontAmbientColor, groundAmbientColor};
    m_groupInteriorData = decltype(m_groupInteriorData)(groupObjects.size());
    for (auto &interiorAmbientBlock : m_groupInteriorData) {
         interiorAmbientBlock.uAmbientColorAndIsExteriorLit = mathfu::vec4(ambientColor, 1.0f);
         interiorAmbientBlock.uHorizontAmbientColor = mathfu::vec4(horizontAmbientColor, 1.0f);
         interiorAmbientBlock.uGroundAmbientColor = mathfu::vec4(groundAmbientColor, 1.0f);
    }
    m_interiorAmbientsChanged = true;
}

std::shared_ptr<CWmoNewLight> WmoObject::getNewLight(int index) {
    if (index > m_newLights.size()) return nullptr;

    return m_newLights[index];
}
void WmoObject::setInteriorAmbientColor(int groupIndex,
                                        bool isExteriorLighted,
                                        const mathfu::vec3 &ambient,
                                        const mathfu::vec3 &horizontAmbient,
                                        const mathfu::vec3 &groundAmbient)
{
    assert(groupIndex < m_groupInteriorData.size());

    auto &interiorAmbientBlock = m_groupInteriorData[groupIndex];
    interiorAmbientBlock.uAmbientColorAndIsExteriorLit = mathfu::vec4(ambient, isExteriorLighted ? 1.0 : 0.0);
    interiorAmbientBlock.uHorizontAmbientColor = mathfu::vec4(horizontAmbient, 1.0f);
    interiorAmbientBlock.uGroundAmbientColor   = mathfu::vec4(groundAmbient, 1.0f);

    m_interiorAmbientsChanged = true;
}


std::shared_ptr<WMOEntityFactory> wmoFactory = std::make_shared<WMOEntityFactory>();