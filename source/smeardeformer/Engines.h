/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * SmearingEngines.h
 */

#ifndef NR_SMEARINGENGINES_H
#define NR_SMEARINGENGINES_H

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
            return true;
        }

        virtual void Destroy() {
        }

        virtual Bool InitParameters(BaseContainer& bc) {
            return true;
        }

        virtual Bool InitData(const BaseContainer& bc, const SmearData& data) {
            return true;
        }

        virtual Bool Init(const SmearData& data, const SmearHistory& history) {
            return true;
        }

        virtual void Free() {
        }

        virtual Bool EnhanceDescription(Description* desc) {
            nr::ExtendDescription(desc, GetId(), GetBase());
            return true;
        }

        virtual Bool GetEnabling(Int32 itemid, const BaseContainer& data, const BaseContainer& itemdesc) {
            return true;
        }

        Int32 GetId() const {
            return m_id;
        }

        Int32 GetBase() const {
            return m_base;
        }

        virtual Bool IsInstanceOf(Int32 type) const {
            return type == GetId();
        }

        static BaseEngine* Alloc(Int32 type, Int32 instance_of=0);

        static Bool Realloc(BaseEngine*& engine, Int32 type, Int32 instance_of=0, Bool* reallocated=nullptr);

        static void Free(BaseEngine*& engine);

    private:

        Int32 m_id;
        Int32 m_base;

    };

    typedef BaseEngine* (*EngineAllocator)();

    class WeightingEngine : public BaseEngine {

    public:

        virtual Float WeightVertex(Int32 vertex_index, const SmearData& data, const SmearHistory& history) = 0;

        //| BaseEngine Overrides

        virtual Bool IsInstanceOf(Int32 type) const {
            if (type == ID_WEIGHTINGENGINE) {
                return true;
            }
            return false;
        }

   };

    class SmearingEngine : public BaseEngine {

    public:

        virtual Int32 GetMaxHistoryLevel() const = 0;

        virtual Vector SmearVertex(Int32 vertex_index, Float weight, const SmearData& data, const SmearHistory& history) = 0;

        //| BaseEngine Overrides

        virtual Bool IsInstanceOf(Int32 type) const {
            if (type == ID_SMEARINGENGINE) {
                return true;
            }
            return false;
        }

    };

    struct EnginePlugin {
        Int32 id;
        Int32 base;
        String name;
        EngineAllocator alloc;
    };

    Bool RegisterEnginePlugin(Int32 id, Int32 base, const String& name, const String& desc, EngineAllocator alloc);

    const EnginePlugin* GetFirstEnginePlugin(Int32 instance_of);

    const EnginePlugin* FindEnginePlugin(Int32 type);

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
