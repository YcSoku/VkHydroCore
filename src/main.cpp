#include <vector>
#include <iostream>
#include "HydroCore/Core.h"

namespace NH = NextHydro;

struct Data {
    float value;
};

int main() {

    fs::path jsonPath = RESOURCE_PATH / fs::path("run.hcs.json");

    auto core = new NH::Core();
    std::vector<float_t> outputArray = {};
    std::vector<Data> testArray({
        Data(0.0),
        Data(1.0),
        Data(2.0),
    });

    auto buffer = core->createStorageBuffer(testArray);
//    auto computePipeline = core->createComputePipeline("test.comp.spv");
    auto computePipeline = core->createComputePipeline(jsonPath.c_str());
    auto descriptorSet = core->createDescriptorSets(computePipeline, buffer);
    core->tick(computePipeline, descriptorSet);
    vkDeviceWaitIdle(core->device);
    buffer.readData(outputArray);

    std::cout << "\n==================== Computation Result ====================" << std::endl;
    for (const auto& value : outputArray) {
        std::cout << value << std::endl;
    }

    return 0;
}