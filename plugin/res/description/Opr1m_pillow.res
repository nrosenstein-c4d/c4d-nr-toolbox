// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

CONTAINER Opr1m_pillow {
    NAME Opr1m_pillow;
    INCLUDE Obase;

    GROUP ID_OBJECTPROPERTIES {
        VECTOR PR1M_PILLOW_SIZE { MIN 0 0 0; UNIT METER; CUSTOMGUI SUBDESCRIPTION; };
    }

    INCLUDE Opr1m_complexshape;
}

