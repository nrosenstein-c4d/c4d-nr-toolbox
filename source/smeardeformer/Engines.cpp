/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * SmearingEngines.h
 */

#include <c4d_apibridge.h>
#include "Engines.h"

typedef maxon::BaseArray<EnginePlugin> EnginePluginArray;
static EnginePluginArray g_plugins;

class iEnginePluginIterator {

public:

    iEnginePluginIterator() : m_it(g_plugins.Begin()) { }

    ~iEnginePluginIterator() { }

    const EnginePlugin* GetPtr() const {
        return m_it.GetPtr();
    }

    void operator ++ () {
        m_it++;
    }

    Bool AtEnd() const {
        return m_it == g_plugins.End();
    }

private:

    EnginePluginArray::ConstIterator m_it;

};

EnginePluginIterator* EnginePluginIterator::Alloc() {
    return (EnginePluginIterator*) NewObjClear(iEnginePluginIterator);
}

void EnginePluginIterator::Free(EnginePluginIterator*& it) {
    if (it) {
        DeleteObj((iEnginePluginIterator*&) it);
        it = nullptr;
    }
}

const EnginePlugin* EnginePluginIterator::GetPtr() const {
    return ((iEnginePluginIterator*) this)->GetPtr();
}

void EnginePluginIterator::operator ++ () {
    ((iEnginePluginIterator*) this)->operator ++ ();
}

Bool EnginePluginIterator::AtEnd() const {
    return ((iEnginePluginIterator*) this)->AtEnd();
}



const EnginePlugin* GetFirstEnginePlugin(Int32 instance_of) {
    EnginePluginArray::ConstIterator it = g_plugins.Begin();
    for (; it != g_plugins.End(); it++) {
        const EnginePlugin* pl = it.GetPtr();
        if (instance_of == 0 || pl->base == instance_of || pl->id == instance_of) {
            return pl;
        }
    }
    return nullptr;
}

const EnginePlugin* FindEnginePlugin(Int32 type) {
    EnginePluginArray::ConstIterator it = g_plugins.Begin();
    for (; it != g_plugins.End(); it++) {
        if (it->id == type) {
            return it.GetPtr();
        }
    }
    return nullptr;
}

BaseEngine* BaseEngine::Alloc(Int32 type, Int32 instance_of) {
    const EnginePlugin* plugin = FindEnginePlugin(type);
    BaseEngine* engine = nullptr;
    if (plugin) {
        engine = (plugin->alloc)();
    }
    if (engine) {
        engine->m_id = type;
        engine->m_base = plugin->base;
        Bool success = engine->Construct();
        if (success && instance_of != 0) {
            success = engine->IsInstanceOf(instance_of);
        }
        if (!success) {
            BaseEngine::Free(engine);
        }
    }
    return engine;
}

Bool BaseEngine::Realloc(BaseEngine*& engine, Int32 type, Int32 instance_of, Bool* reallocated) {
    if (engine && engine->GetId() != type) {
        BaseEngine::Free(engine);
    }
    if (!engine) {
        engine = BaseEngine::Alloc(type, instance_of);
        if (reallocated) *reallocated = engine != nullptr;
    }
    return engine != nullptr;
}

void BaseEngine::Free(BaseEngine*& engine) {
    if (engine) {
        engine->Destroy();
        DeleteMem(engine);
        engine = nullptr;
    }
}


Bool RegisterEnginePlugin(Int32 id, Int32 base, const String& name, const String& desc, EngineAllocator alloc) {
    if (!c4d_apibridge::IsEmpty(desc) && !RegisterDescription(id, desc)) {
        return false;
    }

    const EnginePlugin* p = FindEnginePlugin(id);
    if (p) {
        GePrint("Engine Plugin ID " + String::IntToString(id) + " of " + name + " clashes with " + p->name + ".");
        return false;
    }

    EnginePlugin plugin;
    plugin.id = id;
    plugin.base = base;
    plugin.name = name;
    plugin.alloc = alloc;
    iferr (g_plugins.Append(plugin))
        return false;
    return true;
}


