#pragma once

#include<vector_math/vec.hpp>

namespace systems::leal::vector_math {

    template<class DATA_TYPE>
    class Vector4:public Vec<DATA_TYPE,4> {
    public:
        Vector4() = default;
        Vector4(DATA_TYPE x, DATA_TYPE y, DATA_TYPE z, DATA_TYPE w) {
            this->data[0] = x;
            this->data[1] = y;
            this->data[2] = z;
            this->data[3] = w;
        }

    };

}
