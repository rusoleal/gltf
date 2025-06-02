#pragma once

#include<vector_math/vec.hpp>

namespace systems::leal::vector_math {

    template<class DATA_TYPE, uint32_t ROWS, uint32_t COLS>
    class Mat: Vec<DATA_TYPE,ROWS*COLS> {

    };

}