#pragma once

#include <cstdlib>

namespace systems::leal::vector_math {

    template<class DATA_TYPE, uint32_t SIZE>
    class Vec{
    public:
        typedef DATA_TYPE DataType;

        enum Params { Size = SIZE };

        DATA_TYPE data[SIZE];
    };

}
