// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

CONTAINER Opr1m_jet {
    NAME Opr1m_jet;
    INCLUDE Obase;

    GROUP ID_OBJECTPROPERTIES {
        VECTOR PR1M_JET_SIZE { MIN 0 0 0; CUSTOMGUI SUBDESCRIPTION; UNIT METER; }
    }

    INCLUDE Opr1m_complexshape;
}
