//
// Created by Yucheng Soku on 2024/11/6.
//

#ifndef HYDROCOREPLAYER_BLOCK_H
#define HYDROCOREPLAYER_BLOCK_H

#include <map>
#include <vector>
#include <cassert>
#include <iostream>
#include "ValueType.h"
#include "nlohmann/json.hpp"

using Json = nlohmann::json;

namespace NextHydro {

    struct Block {
        size_t size;
        std::unique_ptr<char[]> buffer;

        Block(const Json& typeList, const Json& jsonData);
    };
}

#endif //HYDROCOREPLAYER_BLOCK_H
