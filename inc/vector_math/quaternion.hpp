#pragma once

#include<vector_math/vector4.hpp>

namespace systems::leal::vector_math {

    template<class DATA_TYPE>
    class Quaternion:public Vector4<DATA_TYPE> {
    public:
        Quaternion() = default;
        Quaternion(DATA_TYPE x, DATA_TYPE y, DATA_TYPE z, DATA_TYPE w):Vector4<DATA_TYPE>(x,y,z,w) {}

    };

}
