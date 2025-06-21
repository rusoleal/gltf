#pragma once

#include<vector_math/vec.hpp>

namespace systems::leal::vector_math {

    template<class DATA_TYPE>
    class Vector2:public Vec<DATA_TYPE,2> {
    public:
        Vector2() = default;
        Vector2(DATA_TYPE x, DATA_TYPE y) {
            this->data[0] = x;
            this->data[1] = y;
        }

    };

}
