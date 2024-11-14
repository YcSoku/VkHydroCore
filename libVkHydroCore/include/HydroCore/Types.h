//
// Created by Yucheng Soku on 2024/11/14.
//

#ifndef HYDROCOREPLAYER_TYPES_H
#define HYDROCOREPLAYER_TYPES_H

#include "config.h"

namespace NextHydro {

    union Flag {
        char        c[4];
        float       f;
        uint32_t    u;
    };
}

#endif //HYDROCOREPLAYER_TYPES_H
