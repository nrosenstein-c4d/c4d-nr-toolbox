// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

CONTAINER Opr1m_torusknot {
    NAME Opr1m_torusknot;
    INCLUDE Obase;

    GROUP ID_OBJECTPROPERTIES {
        REAL PR1M_TORUSKNOT_RADIUS { MIN 0; UNIT METER; }
        REAL PR1M_TORUSKNOT_LEFT   { MIN 0; STEP 0.01; }
        REAL PR1M_TORUSKNOT_RIGHT  { MIN 0; STEP 0.01; }
    }

    INCLUDE Opr1m_complexspline;
}
