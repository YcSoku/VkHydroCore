#include <vector>
#include <chrono>
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

    auto start = std::chrono::high_resolution_clock::now();

    core->runScript();

    std::cout << "\n==================== Computation Complete ====================" << std::endl;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Run time: " << duration.count() << "ms" << std::endl;

    // Check result
    auto buffer = core->name_buffer_map["scalarBuffer"].get();
    std::vector<float_t> outputArray;
    buffer->readData(outputArray);

    std::cout << "\n==================== Computation Result ====================" << std::endl;
    outputArray.resize(3);
    for (const auto& value : outputArray) {
        std::cout << value << std::endl;
    }

    return 0;
}
