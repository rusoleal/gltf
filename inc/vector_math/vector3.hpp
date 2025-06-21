#pragma once

#include<vector_math/vec.hpp>

namespace systems::leal::vector_math {


    template<class DATA_TYPE>
    class Vector3:public Vec<DATA_TYPE,3> {
    public:
        Vector3() = default;
        Vector3(DATA_TYPE x, DATA_TYPE y, DATA_TYPE z) {
            this->data[0] = x;
            this->data[1] = y;
            this->data[2] = z;
        }

    };

}
