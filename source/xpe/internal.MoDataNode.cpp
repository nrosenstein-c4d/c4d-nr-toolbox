/**
 * Copyright (C) 2013-2015 Niklas Rosenstein
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 * TODO:
 * - Update referencing object if XPresso changes.
 */

#include <c4d_apibridge.h>
#include <c4d_operatordata.h>
#include <c4d_baseeffectordata.h>
#include <NiklasRosenstein/c4d/cleanup.hpp>
#include <NiklasRosenstein/macros.h>

#include "MoDataNode.h"
#include "XPressoEffector.h"
#include "internal.xpe_group.h"

#include "res/c4d_symbols.h"
#include "res/description/Gvmodata.h"
#include "misc/raii.h"

#define MODATANODE_VERSION 1000

enum PortFlags {
    PortFlags_None = 0,

    PortFlags_In = (1 << 0),
    PortFlags_Out = (1 << 1),
    PortFlags_Both = PortFlags_In | PortFlags_Out,

    PortFlags_EasyIn = (1 << 2),
    PortFlags_EasyOut = (1 << 3),
    PortFlags_EasyBoth = PortFlags_EasyIn | PortFlags_EasyOut,

    PortFlags_EasyShift = 2,

} ENUM_END_FLAGS(PortFlags) ;

struct PortInfo {
    /**
     * ID of the port.
     */
    Int32 id;

    /**
     * Datatype identifier of the port.
     */
    Int32 dtype;

    /**
     * Flags for the ports type. Whether it is an inport, outport or both.
     */
    PortFlags flags;

    /**
     * String-ID of the port. If <= 0, falls back to `id`.
     */
    Int32 ids;

    /**
     * Port-description flags.
     */
    GvPortDescFlags descflags;

    /**
     * Invalidate constructor.
     */
    PortInfo()
    : id(0), dtype(0), flags(PortFlags_None), ids(0), descflags(GV_PORTDESCRIPTION_NONE) { }

    /**
     * Constructor for four items.
     */
    PortInfo(Int32 id, Int32 dtype, PortFlags flags, Int32 ids)
    : id(id), dtype(dtype), flags(flags), ids(ids), descflags(GV_PORTDESCRIPTION_NONE) { }

    /**
     * Constructor for five items.
     */
    PortInfo(Int32 id, Int32 dtype, PortFlags flags, Int32 ids, GvPortDescFlags descflags)
    : id(id), dtype(dtype), flags(flags), ids(ids), descflags(descflags) { }

    /**
     * Return the original name of the port.
     */
    String GetName() const {
        return GeLoadString(ids > 0 ? ids : id);
    }

    /**
     * Return the flags of the port, shifted by `PortFlags_EasyShift`
     * if *easyMode* is passed true.
     */
    PortFlags GetPortFlagsShifted(Bool easyMode) const {
        if (easyMode) {
            return (PortFlags) (flags >> 2);
        }
        else {
            return flags;
        }
    }

    /**
     * Returns true if the passed GvPortIO mode fits with the flags of
     * this port.
     */
    Bool CheckPortIOMode(GvPortIO portIo, Bool easyMode) const {
        PortFlags flags = GetPortFlagsShifted(easyMode);
        switch (portIo) {
            case GV_PORT_INVALID:
                return false;
            case GV_PORT_INPUT:
                return flags & PortFlags_In;
            case GV_PORT_OUTPUT:
                return flags & PortFlags_Out;
            case GV_PORT_INPUT_OR_GEDATA:
                // TODO: Is this handling correct?
                return true;
            default:
                return false;
        }
    }

};

static PortInfo g_ports[] = {
    PortInfo(MODATANODE_INDEX,      DTYPE_LONG,     PortFlags_In   | PortFlags_EasyIn,   IDC_MODATANODE_PORT_INDEX),
    PortInfo(MODATANODE_OBJECT,     DA_ALIASLINK,   PortFlags_Out  | PortFlags_EasyOut,  IDC_MODATANODE_PORT_OBJECT),
    PortInfo(MODATANODE_HOST,       DA_ALIASLINK,   PortFlags_Out  | PortFlags_EasyOut,  IDC_MODATANODE_PORT_HOST),
    PortInfo(MODATANODE_COUNT,      DTYPE_LONG,     PortFlags_Out  | PortFlags_EasyOut,  IDC_MODATANODE_PORT_COUNT),
    PortInfo(MODATANODE_ITERCOUNT,  DTYPE_LONG,     PortFlags_Out  | PortFlags_EasyOut,  IDC_MODATANODE_PORT_ITERCOUNT),
    PortInfo(MODATANODE_FALLOFF,    DTYPE_REAL,     PortFlags_Out  | PortFlags_EasyOut,  IDC_MODATANODE_PORT_FALLOFF),
    PortInfo(MODATANODE_INDEXOUT,   DTYPE_LONG,     PortFlags_Out  | PortFlags_EasyOut,  IDC_MODATANODE_PORT_INDEXOUT),
    PortInfo(MODATANODE_SELWEIGHT,  DTYPE_BOOL,     PortFlags_Out  | PortFlags_EasyOut,  IDC_MODATANODE_PORT_SELWEIGHT),

    PortInfo(MODATA_MATRIX,         DTYPE_MATRIX,   PortFlags_Both | PortFlags_EasyOut,  IDC_MODATANODE_PORT_MATRIX),
    PortInfo(MODATA_COLOR,          DTYPE_VECTOR,   PortFlags_Both | PortFlags_EasyOut,  IDC_MODATANODE_PORT_COLOR),

    PortInfo(MODATA_WEIGHT,         DTYPE_REAL,     PortFlags_Both,                      IDC_MODATANODE_PORT_WEIGHT),
    PortInfo(MODATANODE_EASYWEIGHT, DTYPE_VECTOR,   PortFlags_EasyIn,                    IDC_MODATANODE_PORT_WEIGHT),

    // Special handling in MoDataNodeData::ServeDynamicInport() since the
    // size is not contained explicitly in the MoData, but implictly via the
    // matrix.
    PortInfo(MODATA_SIZE,           DTYPE_VECTOR,   PortFlags_Both | PortFlags_EasyOut,  IDC_MODATANODE_PORT_SIZE),
    PortInfo(MODATA_UVW,            DTYPE_VECTOR,   PortFlags_Both | PortFlags_EasyOut,  IDC_MODATANODE_PORT_UVW),
    PortInfo(MODATA_FLAGS,          DTYPE_LONG,     PortFlags_Both | PortFlags_EasyOut,  IDC_MODATANODE_PORT_FLAGS),
    PortInfo(MODATA_CLONE,          DTYPE_REAL,     PortFlags_Both | PortFlags_EasyOut,  IDC_MODATANODE_PORT_CLONE),
    PortInfo(MODATA_TIME,           DTYPE_REAL,     PortFlags_Both | PortFlags_EasyOut,  IDC_MODATANODE_PORT_TIME),
    PortInfo(MODATA_FALLOFF_WGT,    DTYPE_REAL,     PortFlags_Both | PortFlags_EasyOut,  IDC_MODATANODE_PORT_FALLOFF_WGT),
    // PortInfo(MODATA_SPLINE_SEGMENT, DTYPE_LONG,     PortFlags_Both | PortFlags_EasyOut, IDC_MODATANODE_PORT_SPLINE_SEGMENT),
    // PortInfo(MODATA_GROWTH,         DTYPE_REAL,     PortFlags_Both | PortFlags_EasyOut, IDC_MODATANODE_PORT_GROWTH),

    PortInfo(MODATA_LASTMAT,        DTYPE_MATRIX,   PortFlags_Out | PortFlags_EasyOut,   IDC_MODATANODE_PORT_LASTMAT),
    PortInfo(MODATA_STARTMAT,       DTYPE_MATRIX,   PortFlags_Out | PortFlags_EasyOut,   IDC_MODATANODE_PORT_STARTMAT),
    PortInfo(MODATA_ALT_INDEX,      DTYPE_LONG,     PortFlags_Out | PortFlags_EasyOut,   IDC_MODATANODE_PORT_ALT_INDEX),


    PortInfo(),
};

class MoPortHandler {

public:

    MoPortHandler(Int32 dataId) : m_dataId(dataId) { }

    virtual ~MoPortHandler() { }

    virtual Bool MDToPort(GvPort* port, GvRun* run, MoData* md, Int32 index) = 0;

    virtual Bool PortToMD(GvPort* port, GvRun* run, MoData* md, Int32 index) = 0;

    Int32 GetId() const { return m_dataId; }

protected:

    Int32 m_dataId;

};

template <typename T>
class AwesomeMoPortHandler : public MoPortHandler {

    typedef MoPortHandler super;

public:

    AwesomeMoPortHandler(Int32 dataId, const T& def)
    : super(dataId), m_default(def) { }

    T* EnsureArray(MoData* md) {
        T* array = (T*) md->GetArray(m_dataId);
        if (!array) {
            Int32 index = md->AddArray(m_dataId);
            if (index == NOTOK) {
                // DEBUG-OFF: MoData Array Adding failed.
                GePrint(String(NR_CURRENT_FUNCTION) + ": Adding array failed. [" + String::IntToString(m_dataId) + "]");
                return nullptr;
            }
            array = (T*) md->GetIndexArray(index);
            if (!array) {
                GePrint(String(NR_CURRENT_FUNCTION) + ": Even after adding the array, it could not be retrieved. [" + String::IntToString(m_dataId) + "]");
                return nullptr;
            }
        }
        return array;
    }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, T* array, Int32 index) = 0;

    virtual Bool iPortToMD(GvPort* port, GvRun* run, T* array, Int32 index) = 0;

    //| MoPortHandler Overrides

    virtual Bool MDToPort(GvPort* port, GvRun* run, MoData* md, Int32 index) {
        T* array = EnsureArray(md);
        if (!array) return iMDToPort(port, run, &m_default, 0);
        else return iMDToPort(port, run, array, index);
    }

    virtual Bool PortToMD(GvPort* port, GvRun* run, MoData* md, Int32 index) {
        T* array = EnsureArray(md);
        if (!array) return false;
        else return iPortToMD(port, run, array, index);
    }

protected:

    T m_default;

};

class MoMatrixPortHandler : public AwesomeMoPortHandler<Matrix> {

    typedef AwesomeMoPortHandler<Matrix> super;

public:

    MoMatrixPortHandler(Int32 a, const Matrix& b) : super(a, b) { }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, Matrix* array, Int32 index) {
        return port->SetMatrix(array[index], run);
    }

    virtual Bool iPortToMD(GvPort* port, GvRun* run, Matrix* array, Int32 index) {
        return port->GetMatrix(&array[index], run);
    }

};

class MoVectorPortHandler : public AwesomeMoPortHandler<Vector> {

    typedef AwesomeMoPortHandler<Vector> super;

public:

    MoVectorPortHandler(Int32 a, const Vector& b) : super(a, b) { }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, Vector* array, Int32 index) {
        return port->SetVector(array[index], run);
    }

    virtual Bool iPortToMD(GvPort* port, GvRun* run, Vector* array, Int32 index) {
        return port->GetVector(&array[index], run);
    }

};

class MoIntPortHandler : public AwesomeMoPortHandler<Int32> {

    typedef AwesomeMoPortHandler<Int32> super;

public:

    MoIntPortHandler(Int32 a, const Int32& b) : super(a, b) { }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, Int32* array, Int32 index) {
        return port->SetInteger(array[index], run);
    }

    virtual Bool iPortToMD(GvPort* port, GvRun* run, Int32* array, Int32 index) {
        return port->GetInteger(&array[index], run);
    }

};

class MoRealPortHandler : public AwesomeMoPortHandler<Float> {

    typedef AwesomeMoPortHandler<Float> super;

public:

    MoRealPortHandler(Int32 a, const Float& b) : super(a, b) { }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, Float* array, Int32 index) {
        return port->SetFloat(array[index], run);
    }

    virtual Bool iPortToMD(GvPort* port, GvRun* run, Float* array, Int32 index) {
        return port->GetFloat(&array[index], run);
    }

};

static MoPortHandler** g_portHandlers = nullptr;
static Int32 g_portHandlerCount = 0;

static void InitMoPortHandlers() {
    g_portHandlerCount = 14;
    iferr (g_portHandlers = NewMemClear(MoPortHandler*, g_portHandlerCount))
        CriticalStop("Well shit");

    Int32 i = 0;
    g_portHandlers[i++] = NewObjClear(MoMatrixPortHandler, MODATA_MATRIX, Matrix());
    g_portHandlers[i++] = NewObjClear(MoMatrixPortHandler, MODATA_LASTMAT, Matrix());
    g_portHandlers[i++] = NewObjClear(MoMatrixPortHandler, MODATA_STARTMAT, Matrix());

    g_portHandlers[i++] = NewObjClear(MoVectorPortHandler, MODATA_COLOR, Vector());
    g_portHandlers[i++] = NewObjClear(MoVectorPortHandler, MODATA_SIZE, Vector(1, 1, 1));
    g_portHandlers[i++] = NewObjClear(MoVectorPortHandler, MODATA_UVW, Vector());

    g_portHandlers[i++] = NewObjClear(MoIntPortHandler, MODATA_FLAGS, 0);
    g_portHandlers[i++] = NewObjClear(MoIntPortHandler, MODATA_SPLINE_SEGMENT, -1);
    g_portHandlers[i++] = NewObjClear(MoIntPortHandler, MODATA_ALT_INDEX, -1);

    g_portHandlers[i++] = NewObjClear(MoRealPortHandler, MODATA_WEIGHT, 1.0);
    g_portHandlers[i++] = NewObjClear(MoRealPortHandler, MODATA_CLONE, 0.0);
    g_portHandlers[i++] = NewObjClear(MoRealPortHandler, MODATA_TIME, 0.0);
    g_portHandlers[i++] = NewObjClear(MoRealPortHandler, MODATA_FALLOFF_WGT, 1.0);
    g_portHandlers[i++] = NewObjClear(MoRealPortHandler, MODATA_GROWTH, 0.0);
}

static void FreeMoPortHandlers() {
    for (Int32 i=0; i < g_portHandlerCount; i++) {
        DeleteObj(g_portHandlers[i]);
    }
    DeleteMem(g_portHandlers);
    g_portHandlerCount = 0;
}

MoPortHandler* FindMoPortHandler(Int32 dataId) {
    for (Int32 i=0; i < g_portHandlerCount; i++) {
        MoPortHandler* handler = g_portHandlers[i];
        if (handler && handler->GetId() == dataId) return handler;
    }
    return nullptr;
}


static PortInfo* FindPortInformation(Int32 id) {
    for (PortInfo* port=g_ports; port->id != 0; port++) {
        if (port->id == id) return port;
    }
    return nullptr;
}

static Bool NodeHasPort(GvNode* node, Int32 id, GvPortIO portIo) {
    Int32 count = 0;
    if (portIo == GV_PORT_INPUT) count = node->GetInPortCount();
    else if (portIo == GV_PORT_OUTPUT) count = node->GetOutPortCount();
    else return 0;

    for (Int32 i=0; i < count; i++) {
        GvPort* port = nullptr;
        if (portIo == GV_PORT_INPUT) port = node->GetInPort(i);
        else if (portIo == GV_PORT_OUTPUT) port = node->GetOutPort(i);
        if (port && port->GetMainID() == id) return true;
    }

    return false;
}

static GeData GetPortGeData(GvNode* node, GvPort* port, GvRun* run, Bool* success) {
    if (success) *success = false;
    if (!node || !port) return GeData();

    GvPortDescription pd;
    if (!node->GetPortDescription(port->GetIO(), port->GetMainID(), &pd)) return GeData();

    GvDataInfo* info = GvGetWorld()->GetDataTypeInfo(pd.data_id);
    if (!info) return GeData();

    GvDynamicData data;
    GvAllocDynamicData(node, data, info);

    GeData result;
    if (port->GetData(data.data, data.info->value_handler->value_id, run)) {
        CUSTOMDATATYPEPLUGIN* pl = FindCustomDataTypePlugin(data.info->data_handler->data_id);
        if (pl && CallCustomDataType(pl, ConvertGvToGeData)(data.data, 0, result)) {
            if (success) *success = true;
        }
    }
    GvFreeDynamicData(data);
    return result;
}

static Bool SetPortGeData(const GeData& ge_data, GvNode* node, GvPort* port, GvRun* run) {
    if (!node || !port || !run) return false;

    GvPortDescription pd;
    if (!node->GetPortDescription(port->GetIO(), port->GetMainID(), &pd)) return false;

    GvDataInfo* info = GvGetWorld()->GetDataTypeInfo(pd.data_id);
    if (!info) return false;

    GvDynamicData data;
    GvAllocDynamicData(node, data, info);

    CUSTOMDATATYPEPLUGIN* pl = FindCustomDataTypePlugin(data.info->data_handler->data_id);
    Bool ok = true;
    ok = ok && pl;
    ok = ok && CallCustomDataType(pl, ConvertGeDataToGv)(ge_data, data.data, 0);
    ok = ok && port->SetData(data.data, data.info->value_handler->value_id, run);
    GvFreeDynamicData(data);
    return ok;
}


class MoDataNodeData : public GvOperatorData {

    typedef GvOperatorData super;

public:

    static NodeData* Alloc() { return NewObjClear(MoDataNodeData); }

    MoDataNodeData() : super() { }

    virtual ~MoDataNodeData() { }

    //| GvOperatorData Overrides

    virtual Bool iGetPortDescription(GvNode* node, GvPortIO portIo, Int32 id, GvPortDescription* pd) {
        if (!node || !pd) return false;
        Bool easyMode = GetEasyMode(node);

        PortInfo* p = FindPortInformation(id);
        if (p && p->CheckPortIOMode(portIo, easyMode)) {
            pd->name = p->GetName();
            pd->flags = p->descflags;
            pd->data_id = p->dtype;
            return true;
        }
        return false;
    }

    virtual Bool iCreateOperator(GvNode* node) {
        if (!node) return false;

        node->SetParameter(MODATANODE_EASYMODE, GeData(true), DESCFLAGS_SET_0);

        Bool ok = true;
        ok = ok && node->AddPort(GV_PORT_INPUT, MODATANODE_INDEX) != nullptr;
        ok = ok && node->AddPort(GV_PORT_OUTPUT, MODATANODE_HOST) != nullptr;
        ok = ok && node->AddPort(GV_PORT_OUTPUT, MODATANODE_OBJECT) != nullptr;
        ok = ok && node->AddPort(GV_PORT_OUTPUT, MODATANODE_COUNT) != nullptr;
        return ok;
    }

    virtual Int32 FillPortsMenu(GvNode* node, BaseContainer& names, BaseContainer& ids,
            GvValueID valueType, GvPortIO portIo, Int32 firstMenuId) {
        if (!node) return false;
        switch (portIo) {
            case GV_PORT_INPUT:
            case GV_PORT_OUTPUT:
                break; // continue with execution
            case GV_PORT_INVALID:
            case GV_PORT_INPUT_OR_GEDATA:
            default:
                return true; // TODO: unkown type, dunno how to handle
        }

        // Check if the node is in easy-mode.
        Bool easyMode = GetEasyMode(node);

        Int32 itemCount = 0;
        for (PortInfo* port=g_ports; port->id != 0; port++) {
            if (!port->CheckPortIOMode(portIo, easyMode)) continue;

            String name = port->GetName();
            if (NodeHasPort(node, port->id, portIo)) name += "&d&";

            names.SetString(firstMenuId, name);
            ids.SetInt32(firstMenuId, port->id);
            firstMenuId++;
            itemCount++;
        }

        return itemCount;
    }

    virtual Bool AddToCalculationTable(GvNode* node, GvRun* run) {
        if (!node || !run || !m_execInfo.md) return false;
        return run->AddNodeToCalculationTable(node);
    }

    virtual Bool InitCalculation(GvNode* node, GvCalc* calc, GvRun* run) {
        if (!node || !calc || !run) return false;

        // Retrieve the Execution-Info.
        m_execInfo.Clean();
        XPressoEffector* effector = GetNodeXPressoEffector(node);
        if (effector) {
            effector->GetExecInfo(&m_execInfo);
        }
        else {
            return false;
        }

        if (m_execInfo.easyOn) {
            *m_execInfo.easyOn = GetEasyMode(node);
        }

        if (!GvBuildValuesTable(node, m_values, calc, run, GV_EXISTING_PORTS)) {
            return false;
        }
        return super::InitCalculation(node, calc, run);
    }

    virtual void FreeCalculation(GvNode* node, GvCalc* calc) {
        GvFreeValuesTable(node, m_values);
        super::FreeCalculation(node, calc);
    }

    virtual Bool Calculate(GvNode* node, GvPort* outPort, GvRun* run, GvCalc* calc) {
        if (!node || !run || !calc || !m_execInfo.md) return false;
        if (!GvCalculateInValuesTable(node, run, calc, m_values)) {
            return false;
        }

        MoData* md = m_execInfo.md;
        BaseObject* gen = m_execInfo.gen;
        C4D_Falloff* falloff = m_execInfo.falloff;
        BaseSelect* sel = m_execInfo.sel;
        XPressoEffector* effector = GetNodeXPressoEffector(node);

        Int32 cloneIndex;
        GvPort* indexPort = node->GetInPortFirstMainID(MODATANODE_INDEX);
        if (indexPort && indexPort->GetInteger(&cloneIndex, run)) ;
        else cloneIndex = 0;

        // Check if the index is in range.
        Int32 cloneCount = md->GetCount();
        if (cloneCount <= 0) {
            return false;
        }

        // Limit the clone index.
        if (cloneIndex < 0) {
            cloneIndex = 0;
        }
        else if (cloneIndex >= cloneCount) {
            cloneIndex = cloneCount - 1;
        }

        if (!outPort) {
            // Iterate over all available in-ports and update the MoData.
            #if API_VERSION < 20000
            Int count = m_values.nr_of_in_values;
            #else
            Int count = m_values.in_values.GetCount();
            #endif
            for (Int32 i=0; i < count; i++) {
                GvValue* value = m_values.GetInValue(i);
                if (!value || value->GetMainID() < MODATANODE_MODATAINDEX_START) {
                    continue;
                }

                GvPort* port = value->GetPort();
                if (!port) {
                    continue;
                }
                ServeDynamicInport(node, port, run, md, cloneIndex);
            }
            return true;
        }
        else {
            // Serve the current output-port.
            switch (outPort->GetMainID()) {
                case MODATANODE_COUNT:
                    return outPort->SetInteger(cloneCount, run);
                case MODATANODE_ITERCOUNT:
                    return outPort->SetInteger(cloneCount - 1, run);
                case MODATANODE_HOST:
                    return SetPortGeData(GeData(effector), node, outPort, run);
                case MODATANODE_OBJECT:
                    return SetPortGeData(GeData(gen), node, outPort, run);
                case MODATANODE_INDEXOUT:
                    return outPort->SetInteger(cloneIndex, run);
                case MODATANODE_FALLOFF: {
                    Float value = 1.0;
                    if (falloff) {
                        MDArray<Matrix> matArray = md->GetMatrixArray(MODATA_MATRIX);
                        MDArray<Float> weightArray = md->GetRealArray(MODATA_WEIGHT);

                        Matrix mat = gen->GetMg() * (matArray ? matArray[cloneIndex] : Matrix());
                        falloff->Sample(mat.off, &value, true, 0.0);
                    }
                    return outPort->SetFloat(value, run);
                }
                case MODATANODE_SELWEIGHT: {
                    Bool value = true;
                    if (sel) {
                        value = sel->IsSelected(cloneIndex);
                    }
                    return outPort->SetBool(value, run);
                }
                default:
                    if (!ServeDynamicOutport(node, outPort, run, md, cloneIndex)) {
                        GePrint("Could not serve out-port " + String::IntToString(outPort->GetMainID()));
                        return false;
                    }
                    return true;
            }
        }

        return false;
    }

    virtual const Vector GetBodyColor(GvNode* node) {
        XPressoEffector* effector = GetNodeXPressoEffector(node);
        if (effector) return super::GetBodyColor(node);
        else return c_wrongBodyColor;
    }

private:

    Bool GetEasyMode(GvNode* node) {
        GeData gEasyMode;
        node->GetParameter(MODATANODE_EASYMODE, gEasyMode, DESCFLAGS_GET_0);
        Bool easyMode = gEasyMode.GetInt32();
        return easyMode;
    }

    Bool ServeDynamicInport(GvNode* node, GvPort* port, GvRun* run, MoData* md, Int32 index) {
        Int32 mainId = port->GetMainID();
        Int32 handlerId = MoDataIdToHandlerId(mainId);

        // If we are in easy mode, we want to use the weight-sub ports.
        if (GetEasyMode(node) && m_execInfo.easyValues && mainId == MODATANODE_EASYWEIGHT) {
            Vector* value = m_execInfo.easyValues + index;
            return port->GetVector(value, run);
        }

        MoPortHandler* handler = FindMoPortHandler(handlerId);
        if (!handler) {
            GePrint(String(NR_CURRENT_FUNCTION) + ": No MoPortHandler found for ID " + String::IntToString(handlerId));
            return false;
        }

        switch (mainId) {
            case MODATA_SIZE: {
                Matrix* matrices = ((AwesomeMoPortHandler<Matrix>*) handler)->EnsureArray(md);
                if (matrices) {
                    Matrix& mat = matrices[index];
                    Vector size;
                    if (!port->GetVector(&size, run)) return false;
                    using namespace c4d_apibridge::M;
                    Mv1(mat) = Mv1(mat).GetNormalized() * size.x;
                    Mv2(mat) = Mv2(mat).GetNormalized() * size.y;
                    Mv3(mat) = Mv3(mat).GetNormalized() * size.z;
                    return true;
                }
                break;
            }
            default:
                return handler->PortToMD(port, run, md, index);
        }
        return false;
    }

    Bool ServeDynamicOutport(GvNode* node, GvPort* port, GvRun* run, MoData* md, Int32 index) {
        Int32 mainId = port->GetMainID();
        Int32 handlerId = MoDataIdToHandlerId(mainId);

        MoPortHandler* handler = FindMoPortHandler(handlerId);
        if (!handler) {
            GePrint(String(NR_CURRENT_FUNCTION) + ": No MoPortHandler found for ID " + String::IntToString(handlerId));
            return false;
        }

        switch (mainId) {
            case MODATA_SIZE: {
                Matrix* matrices = ((AwesomeMoPortHandler<Matrix>*) handler)->EnsureArray(md);
                if (matrices) {
                    using namespace c4d_apibridge::M;
                    Matrix& mat = matrices[index];
                    Vector size(
                            Mv1(mat).GetLength(),
                            Mv2(mat).GetLength(),
                            Mv3(mat).GetLength());
                    return port->SetVector(size, run);
                }
                break;
            }
            default:
                return handler->MDToPort(port, run, md, index);
        }
        return false;
    }

    Int32 MoDataIdToHandlerId(Int32 mainId) {
        // Pre-process the main-id, mapping it to the handler-id.
        Int32 handlerId;
        switch (mainId) {
            case MODATA_SIZE:
                handlerId = MODATA_MATRIX;
                break;
            default:
                handlerId = mainId;
                break;
        }
        return handlerId;
    }

private:

    static const Vector c_wrongBodyColor;

    XPressoEffector_ExecInfo m_execInfo;
    GvValuesInfo m_values;

};

const Vector MoDataNodeData::c_wrongBodyColor = Vector(1.0, 0.2, 0.05);

Bool RegisterMoDataNode() {
    InitMoPortHandlers();
    niklasrosenstein::c4d::cleanup([] {
        FreeMoPortHandlers();
    });
    return GvRegisterOperatorPlugin(
        GvOperatorID(ID_MODATANODE),
        GeLoadString(IDC_MODATANODE_NAME),
        0,
        MoDataNodeData::Alloc,
        "Gvmodata"_s,
        MODATANODE_VERSION,
        ID_GV_OPCLASS_TYPE_GENERAL,
        ID_GV_OPGROUP_TYPE_XPRESSOEFFECTOR,
        0, // owner
        raii::AutoBitmap("icons/Gvmodata.tif"_s));
}
