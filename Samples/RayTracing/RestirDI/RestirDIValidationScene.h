#pragma once

#include "Runtime/Framework/Scene.h"
#include "Runtime/Graphics/Device.h"

#include <memory>
#include <string>
#include <vector>

namespace DSM::RestirDI {

    struct ValidationSceneState
    {
        std::shared_ptr<Scene> scene{};
        ObjectID movingObject = c_InvalidObjectID;
        ObjectID alphaObject = c_InvalidObjectID;
        ObjectID noShadowObject = c_InvalidObjectID;
        ObjectID noShadowReceiver = c_InvalidObjectID;
        ObjectID shadowProbeLight = c_InvalidObjectID;
        std::vector<ObjectID> analyticLightObjects{};
    };

    [[nodiscard]] ValidationSceneState CreateValidationScene(IDevice* device, std::string& error);

}
