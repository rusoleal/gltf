#pragma once

#include<vector_math/mat.hpp>

namespace systems::leal::vector_math {

    template<class DATA_TYPE>
    class Matrix2:public Mat<DATA_TYPE,2,2> {
    public:
        Matrix2() = default;
    };

}