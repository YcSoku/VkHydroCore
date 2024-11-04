#include <vector>
#include <iostream>
#include "HydroCore/Core.h"

namespace NH = NextHydro;

int main() {

    // Script resource
    fs::path jsonPath = RESOURCE_PATH / fs::path("run.hcs.json");

    // Launch GPGPU core
    auto core = new NH::Core();

    // Parse and run script
    core->parseScript(jsonPath.c_str());
    core->runScript();

    // Check result
    auto buffer = core->bufferMap["buffer1"].get();
    std::vector<float_t> outputArray;
    buffer->readData(outputArray);

    std::cout << "\n==================== Computation Result ====================" << std::endl;
    for (const auto& value : outputArray) {
        std::cout << value << std::endl;
    }

    return 0;
}