/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * SmearingEngines.h
 */

#ifndef NR_SMEARINGENGINES_H
#define NR_SMEARINGENGINES_H

    #include "misc/legacy.h"

    #include "nrUtils/Description.h"

    #include "SmearData.h"
    #include "SmearHistory.h"

    #define ID_WEIGHTINGENGINE 5000
    #define ID_SMEARINGENGINE 6000

    class BaseEngine {

    public:

        BaseEngine() { }

        virtual ~BaseEngine() { }

        virtual Bool Construct() {
            return TRUE;
        }

        virtual void Destroy() {
        }

        virtual Bool InitParameters(BaseContainer& bc) {
            return TRUE;
        }

        virtual Bool InitData(const BaseContainer& bc, const SmearData& data) {
            return TRUE;
        }

        virtual Bool Init(const SmearData& data, const SmearHistory& history) {
            return TRUE;
        }

        virtual void Free() {
        }

        virtual Bool EnhanceDescription(Description* desc) {
            nr::ExtendDescription(desc, GetId(), GetBase());
            return TRUE;
        }

        virtual Bool GetEnabling(LONG itemid, const BaseContainer& data, const BaseContainer& itemdesc) {
            return TRUE;
        }

        LONG GetId() const {
            return m_id;
        }

        LONG GetBase() const {
            return m_base;
        }

        virtual Bool IsInstanceOf(LONG type) const {
            return type == GetId();
        }

        static BaseEngine* Alloc(LONG type, LONG instance_of=0);

        static Bool Realloc(BaseEngine*& engine, LONG type, LONG instance_of=0, Bool* reallocated=NULL);

        static void Free(BaseEngine*& engine);

    private:

        LONG m_id;
        LONG m_base;

    };

    typedef BaseEngine* (*EngineAllocator)();

    class WeightingEngine : public BaseEngine {

    public:

        virtual Real WeightVertex(LONG vertex_index, const SmearData& data, const SmearHistory& history) = 0;

        //| BaseEngine Overrides

        virtual Bool IsInstanceOf(LONG type) const {
            if (type == ID_WEIGHTINGENGINE) {
                return TRUE;
            }
            return FALSE;
        }

   };

    class SmearingEngine : public BaseEngine {

    public:

        virtual LONG GetMaxHistoryLevel() const = 0;

        virtual Vector SmearVertex(LONG vertex_index, Real weight, const SmearData& data, const SmearHistory& history) = 0;

        //| BaseEngine Overrides

        virtual Bool IsInstanceOf(LONG type) const {
            if (type == ID_SMEARINGENGINE) {
                return TRUE;
            }
            return FALSE;
        }

    };

    struct EnginePlugin {
        LONG id;
        LONG base;
        String name;
        EngineAllocator alloc;
    };

    Bool RegisterEnginePlugin(LONG id, LONG base, const String& name, const String& desc, EngineAllocator alloc);

    const EnginePlugin* GetFirstEnginePlugin(LONG instance_of);

    const EnginePlugin* FindEnginePlugin(LONG type);

    class EnginePluginIterator {

        EnginePluginIterator();
        ~EnginePluginIterator();

    public:

        static EnginePluginIterator* Alloc();

        static void Free(EnginePluginIterator*& it);

        const EnginePlugin* operator -> () const { return GetPtr(); }

        const EnginePlugin* GetPtr() const;

        void operator ++ ();

        void operator ++ (int) { operator ++ (); }

        Bool AtEnd() const;

    };


#endif /* NR_SMEARINGDEFORMER_ENGINES_H */
