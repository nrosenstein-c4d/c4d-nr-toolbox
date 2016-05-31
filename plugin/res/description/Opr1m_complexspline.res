// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

CONTAINER Opr1m_complexspline {
    NAME Opr1m_complexspline;
    INCLUDE Obase;

    GROUP ID_OBJECTPROPERTIES {
        LONG PR1M_COMPLEXSPLINE_SEGMENTS { MIN 1; }
        // BOOL PR1M_COMPLEXSPLINE_OPTIMIZE { DEFAULT 1; }
        STATICTEXT PR1M_COMPLEXSPLINE_LASTRUN { }
        SEPARATOR { LINE; }
    }

    INCLUDE Ospline;
    HIDE SPLINEOBJECT_TYPE;
    HIDE SPLINEOBJECT_CLOSED;
}
