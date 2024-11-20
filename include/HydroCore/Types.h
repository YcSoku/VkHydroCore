//
// Created by Yucheng Soku on 2024/11/14.
//

#ifndef HYDROCOREPLAYER_TYPES_H
#define HYDROCOREPLAYER_TYPES_H

#include "config.h"

namespace NextHydro {

    union Flag {
        float       f;
        uint32_t    u;
        char        c[4];
    };
}

#endif //HYDROCOREPLAYER_TYPES_H
