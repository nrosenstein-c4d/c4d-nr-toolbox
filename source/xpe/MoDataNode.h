/**
 * Copyright (C) 2013-2015 Niklas Rosenstein
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 */

#ifndef NR_MODATANODE_H
#define NR_MODATANODE_H

    #include <c4d_baseeffectordata.h>

    enum {
        ID_MODATANODE = 1030788,

        MODATANODE_MODATAINDEX_START = MODATA_MATRIX,
        MODATANODE_EASYWEIGHT = MODATA_MATRIX + 10000,              // Vector [in]

        // Smaller than MODATA_MATRIX, which is the first MODATA_ item.
        MODATANODE_INDEX = 35000,                                   // Int32 [in]
        MODATANODE_OBJECT,                                          // LINK [out]
        MODATANODE_HOST,                                            // LINK [out]
        MODATANODE_COUNT,                                           // Int32 [out]
        MODATANODE_ITERCOUNT,                                       // Int32 [out]
        MODATANODE_FALLOFF,                                         // REAL [out]
        MODATANODE_INDEXOUT,                                        // Int32 [out]
        MODATANODE_SELWEIGHT,                                       // BOOL [out]

    };

#endif /* NR_MODATANODE_H */

