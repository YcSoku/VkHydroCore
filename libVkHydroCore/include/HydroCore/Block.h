//
// Created by Yucheng Soku on 2024/11/6.
//

#ifndef HYDROCOREPLAYER_BLOCK_H
#define HYDROCOREPLAYER_BLOCK_H

#include <map>
#include <vector>
#include <cassert>
#include <iostream>

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

    using F32 = float;
    using Vec2 = Vector<F32, 2>;
    using Vec3 = Vector<F32, 3>;
    using Vec4 = Vector<F32, 4>;
    using Mat4 = Matrix<F32, 4>;

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
    struct TypeInfo<Mat4> {
        static size_t size() {
            return sizeof(float) * 4 * 4;
        }

        static size_t alignment() {
            return 16;
        }
    };

    static std::unordered_map<std::string, std::pair<size_t, size_t>> typeInfoMap = {
            {"F32", {TypeInfo<F32>::size(), TypeInfo<F32>::alignment()}},
            {"Vec2", {TypeInfo<Vec2>::size(), TypeInfo<Vec2>::alignment()}},
            {"Vec3", {TypeInfo<Vec3>::size(), TypeInfo<Vec3>::alignment()}},
            {"Vec4", {TypeInfo<Vec4>::size(), TypeInfo<Vec4>::alignment()}},
            {"Mat4", {TypeInfo<Mat4>::size(), TypeInfo<Mat4>::alignment()}}
    };

    static size_t align_to(size_t offset, size_t alignment) {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    static size_t calculate_dynamic_size(const std::vector<std::string>& typeList) {
        size_t offset = 0;

        for (const auto& type : typeList) {
            auto typeInfo = typeInfoMap[type];
            auto size = typeInfo.first;
            auto alignment = typeInfo.second;

            offset = align_to(offset, alignment);
            offset += size;
        }

        return offset;
    }

    struct Block {
        size_t size;
        std::unique_ptr<char[]> buffer;

        Block(const std::vector<std::string>& typeList, const std::vector<float_t>& data) {

            size_t elementNumPerBlock = 0;
            for (const auto& type : typeList) {
                elementNumPerBlock += typeInfoMap[type].first / sizeof(float_t) ;
            }
            assert(data.size() % elementNumPerBlock == 0);

            // Allocate memory for buffer
            size = 0;
            size_t sizePerBlock = calculate_dynamic_size(typeList);
            size_t blockCount = (data.size() / elementNumPerBlock);
            for (size_t i = 0 ; i < blockCount; ++i) {
                size += sizePerBlock;
                size = align_to(size, 16);
            }
            buffer = std::make_unique<char[]>(size);

            // Fill data
            size_t index = 0;
            size_t offset = 0;
            while(index < data.size()) {
                for (const auto& type: typeList) {
                    auto typeInfo = typeInfoMap[type];
                    size_t typeSize = typeInfo.first;
                    size_t typeAlignment = typeInfo.second;

                    offset = align_to(offset, typeAlignment);
                    std::memmove(buffer.get() + offset, &data[index], typeSize);
                    index += typeSize / sizeof(float);
                    offset += typeSize;
                }
                offset = align_to(offset, 16);
            }
        }
    };
}

#endif //HYDROCOREPLAYER_BLOCK_H
