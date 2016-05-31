CONTAINER Tvcontainer {
    NAME Tvcontainer;
    INCLUDE Tvbase;

    GROUP ID_TVPROPERTIES {
        BOOL TVCONTAINER_DEFAULT { DEFAULT 0; }
        LONG TVCONTAINER_CONDITIONMODE {
            CYCLE {
                TVCONTAINER_CONDITIONMODE_AND;
                TVCONTAINER_CONDITIONMODE_OR;
            }
        }
    }
}