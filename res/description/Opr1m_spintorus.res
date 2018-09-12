// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

CONTAINER Opr1m_spintorus {
    NAME Opr1m_spintorus;
    INCLUDE Obase;

    GROUP ID_OBJECTPROPERTIES {
        REAL PR1M_SPINTORUS_RADIUS { UNIT METER; MIN 0; }
        REAL PR1M_SPINTORUS_PIPERADIUS { UNIT METER; MIN 0; }
    }

    INCLUDE Opr1m_complexshape;
}

