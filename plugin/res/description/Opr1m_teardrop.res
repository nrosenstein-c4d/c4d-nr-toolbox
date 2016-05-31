// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

CONTAINER Opr1m_teardrop {
    NAME Opr1m_teardrop;
    INCLUDE Obase;

    GROUP ID_OBJECTPROPERTIES {
        REAL PR1M_TEARDROP_A { STEP 0.1; DEFAULT 50; }
        REAL PR1M_TEARDROP_B { STEP 0.01; DEFAULT 1;}
        REAL PR1M_TEARDROP_SPHERIFY { MIN 0; MAX 100; STEP 1; UNIT PERCENT; DEFAULT 0; }
        REAL PR1M_TEARDROP_LENGTH { UNIT METER; MIN 0; DEFAULT 200; }
    }

    INCLUDE Opr1m_complexshape;

}

