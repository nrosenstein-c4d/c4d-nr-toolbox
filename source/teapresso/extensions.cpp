/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include <c4d_apibridge.h>
#include "extensions.h"
#include "TeaPresso.h"
#include "utils.h"
#include "res/c4d_symbols.h"

using c4d_apibridge::GetDescriptionID;

// ===========================================================================
// ===== Data Class Definitions ==============================================
// ===========================================================================

class TvContainerData : public TvOperatorData {

public:

    static NodeData* Alloc() { return NewObjClear(TvContainerData); }

    /* TvOperatorData Overrides */

    virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context);

    virtual Bool AskCondition(TvNode* host, TvNode* root, BaseList2D* context);

    virtual Bool PredictContextType(TvNode* host, Int32 type);

    virtual Bool AcceptChild(TvNode* host, TvNode* other);

    /* TvOperatorData Overrides */

    virtual Bool Init(GeListNode* gNode);
};

BaseList2D* TvContainerData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host) return nullptr;
    if (!host->IsEnabled()) return context;

    TvNode* child = host->GetDown();
    while (child && context) {
        context = child->Execute(root, context);
        child = child->GetNext();
    }
    return context;
}

Bool TvContainerData::AskCondition(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host) return false;
    if (!host->IsEnabled()) return true;

    BaseContainer* data = host->GetDataInstance();
    if (!data) return false;

    Bool result = data->GetBool(TVCONTAINER_DEFAULT);
    Int32 mode = data->GetInt32(TVCONTAINER_CONDITIONMODE);

    TvNode* child = host->GetDown();
    if (!child) return result;
    else result = child->AskCondition(root, context);
    child = child->GetNext();

    while (child) {
        Bool temp = child->AskCondition(root, context);
        switch (mode) {
            case TVCONTAINER_CONDITIONMODE_AND:
                result &= temp;
                break;
            case TVCONTAINER_CONDITIONMODE_OR:
                result |= temp;
                break;
        }

        child = child->GetNext();
    }
    return result;
}

Bool TvContainerData::PredictContextType(TvNode* host, Int32 type) {
    if (!host) return false;
    TvNode* parent = host->GetUp();
    if (!parent) return false;
    else return parent->PredictContextType(type);
}

Bool TvContainerData::AcceptChild(TvNode* host, TvNode* other) {
    if (!host || !other) return false;
    TvNode* parent = host->GetUp();
    if (!parent) return true;
    else return parent->AcceptChild(other);
}

Bool TvContainerData::Init(GeListNode* gNode) {
    if (!gNode || !TvOperatorData::Init(gNode)) return false;
    TvNode* node = (TvNode*) gNode;
    BaseContainer* data = node->GetDataInstance();
    if (!data) return false;
    data->SetBool(TVCONTAINER_DEFAULT, true);
    data->SetInt32(TVCONTAINER_CONDITIONMODE, TVCONTAINER_CONDITIONMODE_AND);
    return true;
}


/*base */ class TvIteratorData : public TvOperatorData {

public:

    /* TvOperatorData Overrides */

    virtual Bool AcceptChild(TvNode* host, TvNode* other);

};

Bool TvIteratorData::AcceptChild(TvNode* host, TvNode* other) {
    if (!host || !other) return false;
    TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(other);
    return plug->info & TVPLUGIN_EXPRESSION;
}


class TvEachDocumentData : public TvIteratorData {

public:

    static NodeData* Alloc() { return NewObjClear(TvEachDocumentData); }

    /* TvOperatorData Overrides */

    virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context);

    virtual Bool PredictContextType(TvNode* host, Int32 type);

};

BaseList2D* TvEachDocumentData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host) return nullptr;
    BaseDocument* doc = GetFirstDocument();
    TvNode* child = host->GetDown();
    while (doc) {
        BaseList2D* currentContext = doc;
        TvNode* current = child;
        while (current) {
            currentContext = current->Execute(root, currentContext);
            current = current->GetNext();
        }
        doc = doc->GetNext();
    }
    return context;
}

Bool TvEachDocumentData::PredictContextType(TvNode* host, Int32 type) {
    if (!host) return false;
    return type == Tbasedocument;
}


class TvEachObjectData : public TvIteratorData {

    BaseObject* Apply(TvNode* host, TvNode* root, BaseObject* obj);

public:

    static NodeData* Alloc() { return NewObjClear(TvEachObjectData); }

    /* TvOperatorData Overrides */

    virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context);

    virtual Bool AcceptParent(TvNode* host, TvNode* newParent);

    virtual Bool PredictContextType(TvNode* host, Int32 type);

};

BaseObject* TvEachObjectData::Apply(TvNode* host, TvNode* root, BaseObject* obj) {
    if (!obj) return nullptr;
    BaseList2D* child;

    child = host->GetDown();
    while (child && obj) {
        obj = (BaseObject*) ((TvNode*)child)->Execute(root, obj);
        child = child->GetNext();
    }

    child = obj->GetDown();
    while (child) {
        child = Apply(host, root, (BaseObject*) child);
        if (child)
            child = child->GetNext();
    }

    return obj;
}

BaseList2D* TvEachObjectData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host || !context) return nullptr;
    if (!host->IsEnabled() || !host->GetDown()) return context;
    if (!context->IsInstanceOf(Tbasedocument)) return context;
    BaseDocument* doc = (BaseDocument*) context;

    BaseObject* obj = doc->GetFirstObject();
    while (obj) {
        obj = Apply(host, root, obj);
        if (obj) obj = obj->GetNext();
    }

    return context;
}

Bool TvEachObjectData::AcceptParent(TvNode* host, TvNode* newParent) {
    if (!host || !newParent) return false;
    return newParent->PredictContextType(Tbasedocument);
}

Bool TvEachObjectData::PredictContextType(TvNode* host, Int32 type) {
    if (!host) return false;
    return type == Obase;
}


class TvEachTagData : public TvIteratorData {

public:

    static NodeData* Alloc() { return NewObjClear(TvEachTagData); }

    /* TvOperatorData Overrides */

    virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context);

    virtual Bool AcceptParent(TvNode* host, TvNode* newParent);

    virtual Bool PredictContextType(TvNode* host, Int32 type);

};

BaseList2D* TvEachTagData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host || !context) return nullptr;
    if (!host->IsEnabled() || !host->GetDown()) return context;
    if (!context->IsInstanceOf(Obase)) return context;
    BaseObject* op = (BaseObject*) context;

    BaseTag* tag = op->GetFirstTag();
    while (tag) {
        BaseList2D* cContext = context;
        TvNode* cNode = host->GetDown();
        while (cNode) {
            cContext = cNode->Execute(root, tag);
            cNode = cNode->GetNext();
        }
        tag = tag->GetNext();
    }

    return context;
}

Bool TvEachTagData::AcceptParent(TvNode* host, TvNode* newParent) {
    if (!host || !newParent) return false;
    return newParent->PredictContextType(Obase);
}

Bool TvEachTagData::PredictContextType(TvNode* host, Int32 type) {
    if (!host) return false;
    return type == Tbase;
}



class TvConditionData : public TvOperatorData {

public:

    static NodeData* Alloc() { return NewObjClear(TvConditionData); }

    /* TvOperatorData Overrides */

    virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context);

    virtual Bool PredictContextType(TvNode* host, Int32 type);

    virtual Bool AcceptChild(TvNode* host, TvNode* other);

    virtual Bool AllowRemoveChild(TvNode* host, TvNode* child);

    /* NodeData Overrides */

    virtual Bool Init(GeListNode* node);

    virtual Bool Message(GeListNode* gNode, Int32 type, void* pData);

};

BaseList2D* TvConditionData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host || !context) return nullptr;
    if (!host->IsEnabled()) return context;

    TvNode* ifNode = host->GetDown();
    if (!ifNode) return context;
    TvNode* thenNode = ifNode->GetNext();
    if (!thenNode) return context;
    TvNode* elseNode = thenNode->GetNext();

    if (ifNode->AskCondition(root, context)) {
        return thenNode->Execute(root, context);
    }
    else {
        return elseNode->Execute(root, context);
    }
}

Bool TvConditionData::PredictContextType(TvNode* host, Int32 type) {
    if (!host) return false;
    TvNode* parent = host->GetUp();
    if (!parent) return false;
    else return parent->PredictContextType(type);
}

Bool TvConditionData::AcceptChild(TvNode* host, TvNode* other) {
    return TvFindChildDirect(host, other);
}

Bool TvConditionData::AllowRemoveChild(TvNode* host, TvNode* child) {
    return false;
}

Bool TvConditionData::Message(GeListNode* gNode, Int32 type, void* pData) {
    if (!gNode) return false;
    TvNode* host = (TvNode*) gNode;
    switch (type) {
        case MSG_TEAPRESSO_SETUP: {
            TvNode* ifNode = TvNode::Alloc(Tvif);
            TvNode* thenNode = TvNode::Alloc(Tvthen);
            TvNode* elseNode = TvNode::Alloc(Tvelse);
            if (ifNode && thenNode && elseNode) {
                ifNode->InsertUnderLast(host);
                thenNode->InsertUnderLast(host);
                elseNode->InsertUnderLast(host);
            }
            return true;
        }
    }
    return TvOperatorData::Message(gNode, type, pData);
}

Bool TvConditionData::Init(GeListNode* gNode) {
    if (!gNode || !TvOperatorData::Init(gNode)) return false;
    /*
    TvNode* node = (TvNode*) gNode;
    BaseContainer* data = node->GetDataInstance();
    if (!data) return false;
    */
    return true;
}


class TvIfData : public TvContainerData {

public:

    static NodeData* Alloc() { return NewObjClear(TvIfData); }

    /* TvOperatorData Overrides */

    virtual Bool AcceptChild(TvNode* host, TvNode* other);

};

Bool TvIfData::AcceptChild(TvNode* host, TvNode* other) {
    if (!host || !other) return false;
    TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(other);
    return plug->info & TVPLUGIN_CONDITION;
}


class TvThenElseData : public TvContainerData {

public:

    static NodeData* Alloc() { return NewObjClear(TvThenElseData); }

    /* TvOperatorData Overrides */

    virtual Bool AcceptChild(TvNode* host, TvNode* other);
};

Bool TvThenElseData::AcceptChild(TvNode* host, TvNode* other) {
    if (!host || !other) return false;
    TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(other);
    return plug->info & TVPLUGIN_EXPRESSION;
}


class TvPrintContextData : public TvOperatorData {

    typedef TvOperatorData super;

    String GetText(TvNode* host, BaseList2D* context, String type);

public:

    static NodeData* Alloc() { return NewObjClear(TvPrintContextData); }

    /* TvOperatorData Overrides */

    virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context);

    virtual Bool AskCondition(TvNode* host, TvNode* root, BaseList2D* context);

    /* NodeData Overrides */

    virtual Bool Init(GeListNode* node);

    virtual Bool GetDEnabling(
            GeListNode* node, const DescID& descid, const GeData& tData,
            DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);

};

String TvPrintContextData::GetText(TvNode* host, BaseList2D* context, String type) {
    BaseContainer* data = host->GetDataInstance();
    if (!data) return "";

    String line = "";
    if (data->GetBool(TVPRINTCONTEXT_PRINTNAME)) {
        line += host->GetName() + " ";
    }
    line += type + ": ";

    // Obtain the name of the context that will be used to be printed
    // out.
    String contextName = "";
    if (!context) {
        contextName = GeLoadString(IDC_PRINTCONTEXT_NOOBJECT);
    }
    else if (context->IsInstanceOf(Tbasedocument)) {
        contextName = ((BaseDocument*) context)->GetDocumentName().GetString();
    }
    else {
        contextName = context->GetName();
    }

    String customText = data->GetString(TVPRINTCONTEXT_CUSTOMTEXT);

    switch (data->GetInt32(TVPRINTCONTEXT_MODE)) {
        case TVPRINTCONTEXT_MODE_CONTEXTNAME:
            line += contextName;
            break;
        case TVPRINTCONTEXT_MODE_CUSTOMTEXT:
            line += customText;
            break;
        case TVPRINTCONTEXT_MODE_COMBINED:
            line += contextName + " " + customText;
            break;
    }

    return line;
}

BaseList2D* TvPrintContextData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host) return nullptr;
    if (!host->IsEnabled()) return context;

    GePrint(GetText(host, context, GeLoadString(IDC_TEXT_INEXPRESSION)));
    return context;
}

Bool TvPrintContextData::AskCondition(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host) return false;
    if (!host->IsEnabled()) return false;

    GePrint(GetText(host, context, GeLoadString(IDC_TEXT_INCONDITION)));
    return false;
}

Bool TvPrintContextData::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return false;
    BaseContainer* data = ((TvNode*) node)->GetDataInstance();
    if (!data) return false;

    data->SetBool(TVPRINTCONTEXT_PRINTNAME, true);
    data->SetInt32(TVPRINTCONTEXT_MODE, TVPRINTCONTEXT_MODE_CONTEXTNAME);
    data->SetString(TVPRINTCONTEXT_CUSTOMTEXT, ""_s);
    return true;
}

Bool TvPrintContextData::GetDEnabling(
        GeListNode* node, const DescID& descid, const GeData& tData,
        DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
    if (!node) return false;
    BaseContainer* data = ((TvNode*) node)->GetDataInstance();
    if (!data) return false;

    Int32 id = TvDescIDLong(descid);
    switch (id) {
        case TVPRINTCONTEXT_CUSTOMTEXT: {
            Int32 mode = data->GetBool(TVPRINTCONTEXT_MODE);
            return mode != TVPRINTCONTEXT_MODE_CONTEXTNAME;
        }
    }

    return super::GetDEnabling(node, descid, tData, flags, itemdesc);
}


class TvCheckTypeData : public TvOperatorData {

public:

    static NodeData* Alloc() { return NewObjClear(TvCheckTypeData); }

    /* TvOperatorData Overrides */

    virtual Bool AskCondition(TvNode* host, TvNode* root, BaseList2D* context);

    virtual Bool OnDescriptionCommand(TvNode* node, const DescriptionCommand& data);

    /* NodeData Overrides */

    virtual Bool Init(GeListNode* node);

};

Bool TvCheckTypeData::AskCondition(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host || !context) return false;
    if (!host->IsEnabled()) return false;
    BaseContainer* data = host->GetDataInstance();
    if (!data) return false;
    Bool result = context->IsInstanceOf(data->GetInt32(TVCHECKTYPE_TYPEID));
    if (host->IsInverted()) result = !result;
    return result;
}

Bool TvCheckTypeData::OnDescriptionCommand(TvNode* node, const DescriptionCommand& data) {
    Int32 id = TvDescIDLong(GetDescriptionID(&data));
    BaseContainer* bc = node->GetDataInstance();
    if (!bc) return false;

    switch (id) {
        case TVCHECKTYPE_BTN_CHOOSE: {
            AutoAlloc<AtomArray> arr;
            if (!arr) return false;

            BaseContainer popup;
            BaseContainer nodes;
            BaseContainer objects;
            BaseContainer tags;
            BaseContainer shaders;
            BaseContainer materials;
            BaseContainer scenehooks;

            nodes.SetString(1, GeLoadString(IDC_NODES));
            FilterPluginList(arr, PLUGINTYPE_NODE, true);
            TvFillPopupContainerBasePlugin(nodes, arr);
            arr->Flush();

            objects.SetString(1, GeLoadString(IDC_OBJECTS));
            FilterPluginList(arr, PLUGINTYPE_OBJECT, true);
            TvFillPopupContainerBasePlugin(objects, arr);
            arr->Flush();

            tags.SetString(1, GeLoadString(IDC_TAGS));
            FilterPluginList(arr, PLUGINTYPE_TAG, true);
            TvFillPopupContainerBasePlugin(tags, arr);
            arr->Flush();

            shaders.SetString(1, GeLoadString(IDC_SHADERS));
            FilterPluginList(arr, PLUGINTYPE_SHADER, true);
            TvFillPopupContainerBasePlugin(shaders, arr);
            arr->Flush();

            materials.SetString(1, GeLoadString(IDC_MATERIALS));
            FilterPluginList(arr, PLUGINTYPE_MATERIAL, true);
            TvFillPopupContainerBasePlugin(materials, arr);
            arr->Flush();

            scenehooks.SetString(1, GeLoadString(IDC_SCENEHOOKS));
            FilterPluginList(arr, PLUGINTYPE_SCENEHOOK, true);
            TvFillPopupContainerBasePlugin(scenehooks, arr);
            arr->Flush();

            popup.SetContainer(10, nodes);
            popup.SetContainer(11, objects);
            popup.SetContainer(12, tags);
            popup.SetContainer(13, shaders);
            popup.SetContainer(14, materials);
            popup.SetContainer(15, scenehooks);
            Int32 id = ShowPopupMenu(nullptr, MOUSEPOS, MOUSEPOS, popup);

            if (id > 100) {
                bc->SetInt32(TVCHECKTYPE_TYPEID, id);
                TvUpdateTreeViews();
            }
        }
    }

    return true;
}

Bool TvCheckTypeData::Init(GeListNode* gNode) {
    if (!gNode || !TvOperatorData::Init(gNode)) return false;
    TvNode* node = (TvNode*) gNode;
    BaseContainer* data = node->GetDataInstance();
    if (!data) return false;
    return true;
}


class TvIsSelectedData : public TvOperatorData {

public:

    static NodeData* Alloc() { return NewObjClear(TvIsSelectedData); }

    /* TvOperatorData Overrides */

    virtual Bool AskCondition(TvNode* host, TvNode* root, BaseList2D* context);

};

Bool TvIsSelectedData::AskCondition(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host || !context) return false;
    Bool result = context->GetBit(BIT_ACTIVE);
    if (host->IsInverted()) result = !result;
    return result;
}


class TvSelectData : public TvOperatorData {

    typedef TvOperatorData super;

public:

    static NodeData* Alloc() { return NewObjClear(TvSelectData); }

    /* TvOperatorData Overrides */

    virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context);

    /* NodeData Overrides */

    virtual Bool Init(GeListNode* node);

};

BaseList2D* TvSelectData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host) return nullptr;
    if (!root || !context || !host->IsEnabled()) return context;

    BaseContainer* rData = root->GetDataInstance();
    BaseContainer* hData = host->GetDataInstance();
    if (!rData || !hData) return nullptr;

    // Add undo if desired.
    if (rData->GetBool(TVROOT_ADDUNDOS)) {
        BaseDocument* doc = context->GetDocument();
        if (doc)
            doc->AddUndo(UNDOTYPE_BITS, context);
    }

    // Perform the selection.
    if (hData->GetBool(TVSELECT_STATUS)) {
        context->SetBit(BIT_ACTIVE);
    }
    else {
        context->DelBit(BIT_ACTIVE);
    }

    return context;
}

Bool TvSelectData::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return false;
    BaseContainer* data = ((TvNode*) node)->GetDataInstance();
    if (!data) return false;

    data->SetBool(TVSELECT_STATUS, true);
    return true;
}


class TvCurrentDocumentData : public TvThenElseData {

    typedef TvThenElseData super;

    Bool isRoot;

public:

    static NodeData* AllocRoot() { return NewObjClear(TvCurrentDocumentData, true); }

    static NodeData* Alloc() { return NewObjClear(TvCurrentDocumentData, false); }

    TvCurrentDocumentData(Bool isRoot) : super(), isRoot(isRoot) {}

    /* TvOperatorData Overrides */

    virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context);

    virtual Bool PredictContextType(TvNode* host, Int32 type);

    /* NodeData Overrides */

    virtual Bool Init(GeListNode* node);

};

BaseList2D* TvCurrentDocumentData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    if (!host) return nullptr;
    BaseDocument* doc = GetActiveDocument();
    if (isRoot) root = host;
    if (doc) {
        super::Execute(host, root, doc);
    }
    return context;
}

Bool TvCurrentDocumentData::PredictContextType(TvNode* host, Int32 type) {
    if (!host) return false;
    return type == Tbasedocument;
}

Bool TvCurrentDocumentData::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return false;
    BaseContainer* data = ((BaseList2D*) node)->GetDataInstance();
    if (!data) return false;

    data->SetBool(TVROOT_ADDUNDOS, true);
    return true;
}



// ===========================================================================
// ===== Registration Functions ==============================================
// ===========================================================================

static Bool RegisterTvRoot() {
    return TvRegisterOperatorPlugin(
        Tvroot,
        GeLoadString(IDC_TVROOT),
        TVPLUGIN_EXPRESSION,
        TvCurrentDocumentData::AllocRoot,
        "Tvroot"_s,
        AutoBitmap("Tvroot.png"_s),
        0,
        TEAPRESSO_FOLDER_CONTEXTS);
}

static Bool RegisterTvCurrentDocument() {
    return TvRegisterOperatorPlugin(
        Tvcurrentdocument,
        GeLoadString(IDC_TVCURRENTDOCUMENT),
        TVPLUGIN_EXPRESSION,
        TvCurrentDocumentData::Alloc,
        "Tvcurrentdocument"_s,
        AutoBitmap("Tvcurrentdocument.png"_s),
        0,
        TEAPRESSO_FOLDER_CONTEXTS);
}

static Bool RegisterTvContainer() {
    return TvRegisterOperatorPlugin(
        Tvcontainer,
        GeLoadString(IDC_TVCONTAINER),
        TVPLUGIN_EXPRESSION | TVPLUGIN_CONDITION,
        TvContainerData::Alloc,
        "Tvcontainer"_s,
        AutoBitmap("Tvcontainer.png"_s),
        0);
}

static Bool RegisterTvEachDocument() {
    return TvRegisterOperatorPlugin(
        Tveachdocument,
        GeLoadString(IDC_TVEACHDOCUMENT),
        TVPLUGIN_EXPRESSION,
        TvEachDocumentData::Alloc,
        "Tveachdocument"_s,
        AutoBitmap("Tveachdocument.png"_s),
        0,
        TEAPRESSO_FOLDER_ITERATORS);
}

static Bool RegisterTvEachObject() {
    return TvRegisterOperatorPlugin(
        Tveachobject,
        GeLoadString(IDC_TVEACHOBJECT),
        TVPLUGIN_EXPRESSION,
        TvEachObjectData::Alloc,
        "Tveachobject"_s,
        AutoBitmap("Tveachobject.png"_s),
        0,
        TEAPRESSO_FOLDER_ITERATORS);
}

static Bool RegisterTvEachTag() {
    return TvRegisterOperatorPlugin(
        Tveachtag,
        GeLoadString(IDC_TVEACHTAG),
        TVPLUGIN_EXPRESSION,
        TvEachTagData::Alloc,
        "Tveachtag"_s,
        AutoBitmap("Tveachtag.png"_s),
        0,
        TEAPRESSO_FOLDER_ITERATORS);
}

static Bool RegisterTvCondition() {
    return TvRegisterOperatorPlugin(
        Tvcondition,
        GeLoadString(IDC_TVCONDITION),
        TVPLUGIN_EXPRESSION,
        TvConditionData::Alloc,
        "Tvcondition"_s,
        AutoBitmap("Tvcondition.png"_s),
        0);
}

static Bool RegisterIf() {
    return TvRegisterOperatorPlugin(
        Tvif,
        GeLoadString(IDC_TVIF),
        TVPLUGIN_CONDITION | PLUGINFLAG_HIDEPLUGINMENU,
        TvIfData::Alloc,
        "Tvif"_s,
        AutoBitmap("Tvif.png"_s),
        0);
}

static Bool RegisterThen() {
    return TvRegisterOperatorPlugin(
        Tvthen,
        GeLoadString(IDC_TVTHEN),
        TVPLUGIN_EXPRESSION | PLUGINFLAG_HIDEPLUGINMENU,
        TvThenElseData::Alloc,
        "Tvthen"_s,
        AutoBitmap("Tvthen.png"_s),
        0);
}

static Bool RegisterElse() {
    return TvRegisterOperatorPlugin(
        Tvelse,
        GeLoadString(IDC_TVELSE),
        TVPLUGIN_EXPRESSION | PLUGINFLAG_HIDEPLUGINMENU,
        TvThenElseData::Alloc,
        "Tvelse"_s,
        AutoBitmap("Tvelse.png"_s),
        0);
}

static Bool RegisterTvPrintContext() {
    return TvRegisterOperatorPlugin(
        Tvprintcontext,
        GeLoadString(IDC_TVPRINTCONTEXT),
        TVPLUGIN_EXPRESSION | TVPLUGIN_CONDITION,
        TvPrintContextData::Alloc,
        "Tvprintcontext"_s,
        AutoBitmap("Tvprintcontext.png"_s),
        0);
}

static Bool RegisterTvCheckType() {
    return TvRegisterOperatorPlugin(
        Tvchecktype,
        GeLoadString(IDC_TVCHECKTYPE),
        TVPLUGIN_CONDITION,
        TvCheckTypeData::Alloc,
        "Tvchecktype"_s,
        AutoBitmap("Tvchecktype.png"_s),
        0);
}

static Bool RegisterTvIsSelected() {
    return TvRegisterOperatorPlugin(
        Tvisselected,
        GeLoadString(IDC_TVISSELECTED),
        TVPLUGIN_CONDITION,
        TvIsSelectedData::Alloc,
        "Tvisselected"_s,
        AutoBitmap("Tvisselected.png"_s),
        0);
}

static Bool RegisterTvSelect() {
    return TvRegisterOperatorPlugin(
        Tvselect,
        GeLoadString(IDC_TVSELECT),
        TVPLUGIN_EXPRESSION,
        TvSelectData::Alloc,
        "Tvselect"_s,
        AutoBitmap("Tvselect.png"_s),
        0);
}

Bool RegisterTvExtensions() {
    if (TvInstalled()) {
        RegisterTvRoot();
        RegisterTvCurrentDocument();
        RegisterTvContainer();
        RegisterTvEachDocument();
        RegisterTvEachObject();
        RegisterTvEachTag();
        RegisterTvCondition();
        RegisterIf();
        RegisterThen();
        RegisterElse();
        RegisterTvPrintContext();
        RegisterTvCheckType();
        RegisterTvIsSelected();
        RegisterTvSelect();
    }
    return true;
}




