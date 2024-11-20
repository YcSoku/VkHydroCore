//
// Created by Yucheng Soku on 2024/11/8.
//

#ifndef HYDROCOREPLAYER_REFLECTOR_H
#define HYDROCOREPLAYER_REFLECTOR_H

#include <vector>
#include <string>
#include <sstream>
#include <functional>
#include <unordered_map>
#include <rttr/registration>
#include "ValueType.h"
#include "CommandNode.h"

namespace NextHydro {

    class Reflector {
    private:
        std::unordered_map<std::string, std::function<void*(const std::vector<std::string>&)>> constructors;

    public:
        static Reflector& Instance() {
            static Reflector instance;
            return instance;
        }

        template <typename... Args>
        void Register(const std::string& typeName, std::function<void*(Args...)> constructor) {
            constructors[typeName] = [constructor](const std::vector<std::string>& args) -> void* {
                auto convertedArgs = convertArgs<Args...>(args);
                return std::apply(constructor, convertedArgs);
            };
        }

        void* Create(const std::string& typeName, const std::vector<std::string>& args) {
            auto it = constructors.find(typeName);
            if (it != constructors.end()) {
                return it->second(args);
            }
            return nullptr;
        }

    private:
        template <typename T>
        static T convertArg(const std::string& arg) {
            std::istringstream stream(arg);
            T result;
            stream >> result;
            return result;
        }

        template <typename... Args>
        static std::tuple<Args...> convertArgs(const std::vector<std::string>& args) {
            return std::make_tuple(convertArg<Args>(args[std::tuple_size_v<std::tuple<Args...>> - sizeof...(Args)])...);
        }
    };
    static void registerTypes() {
        Reflector::Instance().Register("Vec2", std::function<void*(float, float)>([](float x, float y) { return new Vec2(x, y); }));
        Reflector::Instance().Register("Vec3", std::function<void*(float, float, float)>([](float x, float y, float z) { return new Vec3(x, y, z); }));
        Reflector::Instance().Register("Vec4", std::function<void*(float, float, float, float)>([](float x, float y, float z, float w) { return new Vec4(x, y, z, w); }));
    }

    RTTR_REGISTRATION {
        rttr::registration::class_<NextHydro::U32>("U32")
                .constructor<uint32_t>()
                .method("size", &NextHydro::U32::size)
                .method("alignment", &NextHydro::U32::alignment)
                .method("getBufferFromJson", &NextHydro::U32::getBufferFromJson);

        rttr::registration::class_<NextHydro::F32>("F32")
                .constructor<float>()
                .method("size", &NextHydro::F32::size)
                .method("alignment", &NextHydro::F32::alignment)
                .method("getBufferFromJson", &NextHydro::F32::getBufferFromJson);

        rttr::registration::class_<NextHydro::Vec2>("Vec2")
                .constructor<float, float>()
                .method("size", &NextHydro::Vec2::size)
                .method("alignment", &NextHydro::Vec2::alignment)
                .method("getBufferFromJson", &NextHydro::Vec2::getBufferFromJson);

        rttr::registration::class_<NextHydro::Vec3>("Vec3")
                .constructor<float, float, float>()
                .method("size", &NextHydro::Vec3::size)
                .method("alignment", &NextHydro::Vec3::alignment)
                .method("getBufferFromJson", &NextHydro::Vec3::getBufferFromJson);

        rttr::registration::class_<NextHydro::Vec4>("Vec4")
                .constructor<float, float, float, float>()
                .method("size", &NextHydro::Vec4::size)
                .method("alignment", &NextHydro::Vec4::alignment)
                .method("getBufferFromJson", &NextHydro::Vec4::getBufferFromJson);

        rttr::registration::class_<NextHydro::Mat4x4>("Mat4x4")
                .constructor<
                        float, float, float, float,
                        float, float, float, float,
                        float, float, float, float,
                        float, float, float, float>()
                .method("size", &NextHydro::Mat4x4::size)
                .method("alignment", &NextHydro::Mat4x4::alignment)
                .method("getBufferFromJson", &NextHydro::Mat4x4::getBufferFromJson);
    };
}

#endif //HYDROCOREPLAYER_REFLECTOR_H
