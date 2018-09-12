// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

CONTAINER Opr1m_seashell {
    NAME Opr1m_seashell;
    INCLUDE Obase;

    GROUP ID_OBJECTPROPERTIES {
        REAL PR1M_SEASHELL_RADIUS     { MIN 0; UNIT METER; }
        REAL PR1M_SEASHELL_PIPERADIUS { MIN 0; UNIT METER; }
        REAL PR1M_SEASHELL_HEIGHT     { MIN 0; UNIT METER; }
        REAL PR1M_SEASHELL_DEGREES    { MIN 0; UNIT DEGREE; }
    }

    GROUP PR1M_SEASHELL_DETAILS {
        SPLINE PR1M_SEASHELL_RADIUSSPLINE {}
        SEPARATOR {};
        SPLINE PR1M_SEASHELL_PIPESPLINE {}
    }

    INCLUDE Opr1m_complexshape;
}

