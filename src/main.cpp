#include <vector>
#include "core/Core.h"

namespace NH = NextHydro;

int main() {

    auto core = new NH::Core();

    std::vector<float_t> testArray({1.0, 2.0, 3.0});
    auto buffer = core->createStorageBuffer(testArray);
    auto computePipeline = core->createComputePipeline("test.comp.spv");
    return 0;
}