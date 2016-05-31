// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

CONTAINER Opr1m_room {
    NAME Opr1m_room;
    INCLUDE Obase;

    GROUP ID_OBJECTPROPERTIES {
        GROUP {
            LAYOUTGROUP;
            COLUMNS 3;
            GROUP {
                VECTOR PR1M_ROOM_SIZE { MIN 0 0 0; UNIT METER; CUSTOMGUI SUBDESCRIPTION; };
            }
            GROUP {
                VECTOR PR1M_ROOM_THICKNESSP { MIN 0 0 0; UNIT METER; CUSTOMGUI SUBDESCRIPTION; };
            }
            GROUP {
                VECTOR PR1M_ROOM_THICKNESSN { MIN 0 0 0; UNIT METER; CUSTOMGUI SUBDESCRIPTION; };
            }
        }
        BOOL PR1M_ROOM_DOFILLET {};
        REAL PR1M_ROOM_FILLETRAD { MIN 0; UNIT METER; };
        LONG PR1M_ROOM_FILLETSUB { MIN 1; };
    }
    GROUP PR1M_ROOM_GWORKFLOW {
        DEFAULT 1;
        BOOL PR1M_ROOM_DISPLAYTHICKNESSHANDLES {};
        LONG PR1M_ROOM_TEXTUREMODE {
            CYCLE {
                PR1M_ROOM_TEXTUREMODE_SIMPLE;
                PR1M_ROOM_TEXTUREMODE_EXTENDED;
            }
        };
    }
}

