#pragma once

#include<vector_math/mat.hpp>

namespace systems::leal::vector_math {

    template<class DATA_TYPE>
    class Matrix3:public Mat<DATA_TYPE,3,3> {
    public:
        Matrix3() = default;

    };

}