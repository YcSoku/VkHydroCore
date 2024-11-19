//
// Created by Yucheng Soku on 2024/11/9.
//

#include "HydroCore/Block.h"
#include "HydroCore/Reflector.h"

namespace NextHydro {

    // Helpers ////////////////////////////////////////////////////////////////////////////////////////////////////
    size_t align_to(size_t offset, size_t alignment) {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    size_t calculate_dynamic_size(const std::vector<std::string>& typeList) {
        size_t offset = 0;

        for (const auto& typeName : typeList) {

            auto type = rttr::type::get_by_name(typeName);
            size_t typeSize = type.get_method("size").invoke({}).get_value<size_t>();
            size_t typeAlignment = type.get_method("alignment").invoke({}).get_value<size_t>();

            offset = align_to(offset, typeAlignment);
            offset += typeSize;
        }
        return offset;
    }

    // Block ////////////////////////////////////////////////////////////////////////////////////////////////////

    Block::Block(const Json &typeList, const Json &jsonData) {

        bool needFilling = jsonData.is_array();
        uint32_t typeListLength = typeList.is_array() ? typeList.size() : 1;
        size_t dataLength = needFilling ? jsonData.size() : jsonData["length"].get<size_t>();

        // Check if jsonData is suitable for block size
        assert(dataLength % typeListLength == 0);

        // Allocate memory for buffer
        size = 0;
        size_t sizePerBlock;
        if (typeList.is_array()) {
            sizePerBlock = calculate_dynamic_size(typeList);
        } else {
            auto type = rttr::type::get_by_name(typeList);
            sizePerBlock = type.get_method("size").invoke({}).get_value<size_t>();
        }
        size_t blockCount = dataLength / typeListLength;
        for (size_t i = 0; i < blockCount; ++i) {
            size += sizePerBlock;
            size = align_to(size, 16);
        }
        buffer = std::make_unique<char[]>(size);

        // Need to be filled or not
        if (!needFilling) return;

        // Fill data
        size_t index =0;
        size_t offset = 0;
        if (typeList.is_array()) {
            while(index < dataLength) {
                for (const auto& typeName: typeList) {
                    auto type = rttr::type::get_by_name(typeName);
                    size_t typeSize = type.get_method("size").invoke({}).get_value<size_t>();
                    size_t typeAlignment = type.get_method("alignment").invoke({}).get_value<size_t>();
                    auto data = type.get_method("getBufferFromJson").invoke({}, jsonData, index).get_value<std::vector<char>>();

                    offset = align_to(offset, typeAlignment);

                    std::memcpy(buffer.get() + offset, data.data(), typeSize);
                    offset += typeSize;
                }
                offset = align_to(offset, 16);
            }
        } else {
            while(index < dataLength) {
                auto type = rttr::type::get_by_name(typeList);
                size_t typeSize = type.get_method("size").invoke({}).get_value<size_t>();
                size_t typeAlignment = type.get_method("alignment").invoke({}).get_value<size_t>();
                auto data = type.get_method("getBufferFromJson").invoke({}, jsonData, index).get_value<std::vector<char>>();
                offset = align_to(offset, typeAlignment);
                std::memcpy(buffer.get() + offset, data.data(), typeSize);
                offset += typeSize;
            }
        }
    }
}