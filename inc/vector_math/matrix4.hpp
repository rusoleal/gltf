#pragma once

#include<vector_math/mat.hpp>

namespace systems::leal::vector_math {

    template<class DATA_TYPE>
    class Matrix4:public Mat<DATA_TYPE,4,4> {
    public:
        Matrix4() = default;
        Matrix4(DATA_TYPE buffer[16]) {
            memcpy(this->data,buffer,sizeof(DATA_TYPE)*16);
        }

        static Matrix4<DATA_TYPE> identity() {
            return (DATA_TYPE[]){
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0,
            };
        }
    };

}
