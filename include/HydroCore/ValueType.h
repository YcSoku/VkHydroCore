//
// Created by Yucheng Soku on 2024/11/8.
//

#ifndef HYDROCOREPLAYER_VALUETYPE_H
#define HYDROCOREPLAYER_VALUETYPE_H

#include <array>
#include <iostream>
#include "nlohmann/json.hpp"

using Json = nlohmann::json;
namespace NextHydro {

    template <typename T, size_t N>
    struct Vector {
        T data[N];

        Vector() {
            for (size_t i = 0; i < N; ++i) {
                data[i] = 0;
            }
        }

        Vector(std::initializer_list<T> values) {
            size_t i = 0;
            for (auto val : values) {
                if (i < N) {
                    data[i] = val;
                    ++i;
                }
            }
        }

        Vector<T, N> operator+(const Vector<T, N>& other) const {
            Vector<T, N> result;
            for (size_t i = 0; i < N; ++i) {
                result.data[i] = data[i] + other.data[i];
            }
            return result;
        }

        Vector<T, N> operator-(const Vector<T, N>& other) const {
            Vector<T, N> result;
            for (size_t i = 0; i < N; ++i) {
                result.data[i] = data[i] - other.data[i];
            }
            return result;
        }

        Vector<T, N> operator*(T scalar) const {
            Vector<T, N> result;
            for (size_t i = 0; i < N; ++i) {
                result.data[i] = data[i] * scalar;
            }
            return result;
        }

        T dot(const Vector<T, N>& other) const {
            T result = 0;
            for (size_t i = 0; i < N; ++i) {
                result += data[i] * other.data[i];
            }
            return result;
        }

        friend std::ostream& operator<<(std::ostream& os, const Vector<T, N>& v) {
            os << "(";
            for (size_t i = 0; i < N; ++i) {
                os << v.data[i];
                if (i < N - 1) os << ", ";
            }
            os << ")";
            return os;
        }
    };

    template <typename T, size_t N>
    struct Matrix {
        T data[N][N];

        Matrix() {
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = 0; j < N; ++j) {
                    data[i][j] = 0;
                }
            }
        }

        Matrix<T, N> operator*(const Matrix<T, N>& other) const {
            Matrix<T, N> result;
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = 0; j < N; ++j) {
                    result.data[i][j] = 0;
                    for (size_t k = 0; k < N; ++k) {
                        result.data[i][j] += data[i][k] * other.data[k][j];
                    }
                }
            }
            return result;
        }

        friend std::ostream& operator<<(std::ostream& os, const Matrix<T, N>& m) {
            os << "[\n";
            for (size_t i = 0; i < N; ++i) {
                os << "  ";
                for (size_t j = 0; j < N; ++j) {
                    os << m.data[i][j];
                    if (j < N - 1) os << ", ";
                }
                os << "\n";
            }
            os << "]";
            return os;
        }
    };

    class U32 {
    public:
        uint32_t x;
        explicit U32(uint32_t x) : x(x) {}

        static size_t size() {
            return sizeof(uint32_t);
        }

        static size_t alignment() {
            return 4;
        }

        static std::vector<char> getBufferFromJson(const Json& jsonDataArray, size_t& index) {

            std::vector<char> buffer(4);
            *reinterpret_cast<uint32_t*>(buffer.data()) = jsonDataArray[index++].get<uint32_t>();
            return buffer;
        }
    };

    class F32 {
    public:
        float x;
        explicit F32(float x) : x(x) {}

        static size_t size() {
            return sizeof(float);
        }

        static size_t alignment() {
            return 4;
        }

        static std::vector<char> getBufferFromJson(const Json& jsonDataArray, size_t& index) {

            std::vector<char> buffer(4);
            *reinterpret_cast<float_t*>(buffer.data()) = jsonDataArray[index++].get<float_t>();
            return buffer;
        }
    };

    class Vec2 {
    public:
        float x, y;
        Vec2(float x, float y) : x(x), y(y) {}

        static size_t size() {
            return sizeof(float) * 2;
        }

        static size_t alignment() {
            return 8;
        }

        static std::vector<char> getBufferFromJson(const Json& jsonDataArray, size_t& index) {

            std::vector<char> buffer(8);
            auto floatBuffer = reinterpret_cast<float_t*>(buffer.data());
            for (int i = 0; i < 2; ++i) {
                floatBuffer[i] = jsonDataArray[index++].get<float_t>();
            }

            return buffer;
        }
    };

    class Vec3 {
    public:
        float x, y, z;
        Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

        static size_t size() {
            return sizeof(float) * 3;
        }

        static size_t alignment() {
            return 16;
        }

        static std::vector<char> getBufferFromJson(const Json& jsonDataArray, size_t& index) {

            std::vector<char> buffer(12);
            auto floatBuffer = reinterpret_cast<float_t*>(buffer.data());
            for (int i = 0; i < 3; ++i) {
                floatBuffer[i] = jsonDataArray[index++].get<float_t>();
            }

            return buffer;
        }
    };

    class Vec4 {
    public:
        float x, y, z, w;
        Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

        static size_t size() {
            return sizeof(float) * 4;
        }

        static size_t alignment() {
            return 16;
        }

        static std::vector<char> getBufferFromJson(const Json& jsonDataArray, size_t& index) {

            std::vector<char> buffer(16);
            auto floatBuffer = reinterpret_cast<float_t*>(buffer.data());
            for (int i = 0; i < 4; ++i) {
                floatBuffer[i] = jsonDataArray[index++].get<float_t>();
            }

            return buffer;
        }
    };

    class Mat4x4 {
    public:
        float data[4][4]{};
        Mat4x4(
                float m00, float m01, float m02, float m03,
                float m10, float m11, float m12, float m13,
                float m20, float m21, float m22, float m23,
                float m30, float m31, float m32, float m33
        )
        {
            data[0][0] = m00; data[0][1] = m01; data[0][2] = m02; data[0][3] = m03;
            data[1][0] = m10; data[1][1] = m11; data[1][2] = m12; data[1][3] = m13;
            data[2][0] = m20; data[2][1] = m21; data[2][2] = m22; data[2][3] = m23;
            data[3][0] = m30; data[3][1] = m31; data[3][2] = m32; data[3][3] = m33;
        }

        static size_t size() {
            return sizeof(float) * 4 * 4;
        }

        static size_t alignment() {
            return 16;
        }

        static std::vector<char> getBufferFromJson(const Json& jsonDataArray, size_t& index) {

            std::vector<char> buffer(64);
            auto floatBuffer = reinterpret_cast<float_t*>(buffer.data());
            for (int i = 0; i < 16; ++i) {
                floatBuffer[i] = jsonDataArray[index++].get<float_t>();
            }

            return buffer;
        }
    };

    template <typename T>
    struct TypeInfo {
        static size_t size() {
            return sizeof(T);
        }

        static size_t alignment() {
            return alignof(T);
        }
    };

    template <>
    struct TypeInfo<U32> {
        static size_t size() {
            return sizeof(uint32_t);
        }

        static size_t alignment() {
            return 4;
        }
    };

    template <>
    struct TypeInfo<F32> {
        static size_t size() {
            return sizeof(float);
        }

        static size_t alignment() {
            return 4;
        }
    };

    template <>
    struct TypeInfo<Vec2> {
        static size_t size() {
            return sizeof(float) * 2;
        }

        static size_t alignment() {
            return 8;
        }
    };

    template <>
    struct TypeInfo<Vec3> {
        static size_t size() {
            return sizeof(float) * 3;
        }

        static size_t alignment() {
            return 16;
        }
    };

    template <>
    struct TypeInfo<Vec4> {
        static size_t size() {
            return sizeof(float) * 4;
        }

        static size_t alignment() {
            return 16;
        }
    };

    template <>
    struct TypeInfo<Mat4x4> {
        static size_t size() {
            return sizeof(float) * 4 * 4;
        }

        static size_t alignment() {
            return 16;
        }
    };

    static std::unordered_map<std::string, std::pair<size_t, size_t>> typeInfoMap = {
            {"U32", {TypeInfo<U32>::size(), TypeInfo<U32>::alignment()}},
            {"F32", {TypeInfo<F32>::size(), TypeInfo<F32>::alignment()}},
            {"Vec2", {TypeInfo<Vec2>::size(), TypeInfo<Vec2>::alignment()}},
            {"Vec3", {TypeInfo<Vec3>::size(), TypeInfo<Vec3>::alignment()}},
            {"Vec4", {TypeInfo<Vec4>::size(), TypeInfo<Vec4>::alignment()}},
            {"Mat4", {TypeInfo<Mat4x4>::size(), TypeInfo<Mat4x4>::alignment()}}
    };
}

#endif //HYDROCOREPLAYER_VALUETYPE_H
