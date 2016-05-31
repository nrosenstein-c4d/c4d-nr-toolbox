CONTAINER Tvbase {
    NAME Tvbase;
	INCLUDE Obaselist;

    HIDE ID_LAYER_LINK;

    GROUP Obaselist {
        LONG TVBASE_COLORMODE {
            CYCLE {
                TVBASE_COLORMODE_AUTO;
                TVBASE_COLORMODE_INHERIT;
                TVBASE_COLORMODE_USE;
            }
        }
        COLOR TVBASE_COLOR { };
        BOOL TVBASE_ENABLED { DEFAULT 1; }
    }
    GROUP ID_TVPROPERTIES {
        DEFAULT 1;
    }
}