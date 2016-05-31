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

#include "base.h"

#include "misc/legacy.h"
#include <c4d_operatordata.h>
#include <c4d_baseeffectordata.h>
#include <nr/c4d/cleanup.h>

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
    LONG id;

    /**
     * Datatype identifier of the port.
     */
    LONG dtype;

    /**
     * Flags for the ports type. Whether it is an inport, outport or both.
     */
    PortFlags flags;

    /**
     * String-ID of the port. If <= 0, falls back to `id`.
     */
    LONG ids;

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
    PortInfo(LONG id, LONG dtype, PortFlags flags, LONG ids)
    : id(id), dtype(dtype), flags(flags), ids(ids), descflags(GV_PORTDESCRIPTION_NONE) { }

    /**
     * Constructor for five items.
     */
    PortInfo(LONG id, LONG dtype, PortFlags flags, LONG ids, GvPortDescFlags descflags)
    : id(id), dtype(dtype), flags(flags), ids(ids), descflags(descflags) { }

    /**
     * Return the original name of the port.
     */
    String GetName() const {
        return GeLoadString(ids > 0 ? ids : id);
    }

    /**
     * Return the flags of the port, shifted by `PortFlags_EasyShift`
     * if *easyMode* is passed TRUE.
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
     * Returns TRUE if the passed GvPortIO mode fits with the flags of
     * this port.
     */
    Bool CheckPortIOMode(GvPortIO portIo, Bool easyMode) const {
        PortFlags flags = GetPortFlagsShifted(easyMode);
        switch (portIo) {
            case GV_PORT_INVALID:
                return FALSE;
            case GV_PORT_INPUT:
                return flags & PortFlags_In;
            case GV_PORT_OUTPUT:
                return flags & PortFlags_Out;
            case GV_PORT_INPUT_OR_GEDATA:
                // TODO: Is this handling correct?
                return TRUE;
            default:
                return FALSE;
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

    MoPortHandler(LONG dataId) : m_dataId(dataId) { }

    virtual ~MoPortHandler() { }

    virtual Bool MDToPort(GvPort* port, GvRun* run, MoData* md, LONG index) = 0;

    virtual Bool PortToMD(GvPort* port, GvRun* run, MoData* md, LONG index) = 0;

    LONG GetId() const { return m_dataId; }

protected:

    LONG m_dataId;

};

template <typename T>
class AwesomeMoPortHandler : public MoPortHandler {

    typedef MoPortHandler super;

public:

    AwesomeMoPortHandler(LONG dataId, const T& def)
    : super(dataId), m_default(def) { }

    T* EnsureArray(MoData* md) {
        T* array = (T*) md->GetArray(m_dataId);
        if (!array) {
            LONG index = md->AddArray(m_dataId);
            if (index == NOTOK) {
                // DEBUG-OFF: MoData Array Adding failed.
                GePrint(__FUNCTION__ ": Adding array failed. [" + LongToString(m_dataId) + "]");
                return NULL;
            }
            array = (T*) md->GetIndexArray(index);
            if (!array) {
                GePrint(__FUNCTION__ ": Even after adding the array, it could not be retrieved. [" + LongToString(m_dataId) + "]");
                return NULL;
            }
        }
        return array;
    }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, T* array, LONG index) = 0;

    virtual Bool iPortToMD(GvPort* port, GvRun* run, T* array, LONG index) = 0;

    //| MoPortHandler Overrides

    virtual Bool MDToPort(GvPort* port, GvRun* run, MoData* md, LONG index) {
        T* array = EnsureArray(md);
        if (!array) return iMDToPort(port, run, &m_default, 0);
        else return iMDToPort(port, run, array, index);
    }

    virtual Bool PortToMD(GvPort* port, GvRun* run, MoData* md, LONG index) {
        T* array = EnsureArray(md);
        if (!array) return FALSE;
        else return iPortToMD(port, run, array, index);
    }

protected:

    T m_default;

};

class MoMatrixPortHandler : public AwesomeMoPortHandler<Matrix> {

    typedef AwesomeMoPortHandler<Matrix> super;

public:

    MoMatrixPortHandler(LONG a, const Matrix& b) : super(a, b) { }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, Matrix* array, LONG index) {
        return port->SetMatrix(array[index], run);
    }

    virtual Bool iPortToMD(GvPort* port, GvRun* run, Matrix* array, LONG index) {
        return port->GetMatrix(&array[index], run);
    }

};

class MoVectorPortHandler : public AwesomeMoPortHandler<Vector> {

    typedef AwesomeMoPortHandler<Vector> super;

public:

    MoVectorPortHandler(LONG a, const Vector& b) : super(a, b) { }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, Vector* array, LONG index) {
        return port->SetVector(array[index], run);
    }

    virtual Bool iPortToMD(GvPort* port, GvRun* run, Vector* array, LONG index) {
        return port->GetVector(&array[index], run);
    }

};

class MoIntPortHandler : public AwesomeMoPortHandler<LONG> {

    typedef AwesomeMoPortHandler<LONG> super;

public:

    MoIntPortHandler(LONG a, const LONG& b) : super(a, b) { }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, LONG* array, LONG index) {
        return port->SetInteger(array[index], run);
    }

    virtual Bool iPortToMD(GvPort* port, GvRun* run, LONG* array, LONG index) {
        return port->GetInteger(&array[index], run);
    }

};

class MoRealPortHandler : public AwesomeMoPortHandler<Real> {

    typedef AwesomeMoPortHandler<Real> super;

public:

    MoRealPortHandler(LONG a, const Real& b) : super(a, b) { }

    virtual Bool iMDToPort(GvPort* port, GvRun* run, Real* array, LONG index) {
        return port->SetReal(array[index], run);
    }

    virtual Bool iPortToMD(GvPort* port, GvRun* run, Real* array, LONG index) {
        return port->GetReal(&array[index], run);
    }

};

static MoPortHandler** g_portHandlers = NULL;
static LONG g_portHandlerCount = 0;

static void InitMoPortHandlers() {
    g_portHandlerCount = 14;
    g_portHandlers = NewMemClear(MoPortHandler*, g_portHandlerCount);

    LONG i = 0;
    g_portHandlers[i++] = gNew(MoMatrixPortHandler, MODATA_MATRIX, Matrix());
    g_portHandlers[i++] = gNew(MoMatrixPortHandler, MODATA_LASTMAT, Matrix());
    g_portHandlers[i++] = gNew(MoMatrixPortHandler, MODATA_STARTMAT, Matrix());

    g_portHandlers[i++] = gNew(MoVectorPortHandler, MODATA_COLOR, Vector());
    g_portHandlers[i++] = gNew(MoVectorPortHandler, MODATA_SIZE, Vector(1, 1, 1));
    g_portHandlers[i++] = gNew(MoVectorPortHandler, MODATA_UVW, Vector());

    g_portHandlers[i++] = gNew(MoIntPortHandler, MODATA_FLAGS, 0);
    g_portHandlers[i++] = gNew(MoIntPortHandler, MODATA_SPLINE_SEGMENT, -1);
    g_portHandlers[i++] = gNew(MoIntPortHandler, MODATA_ALT_INDEX, -1);

    g_portHandlers[i++] = gNew(MoRealPortHandler, MODATA_WEIGHT, 1.0);
    g_portHandlers[i++] = gNew(MoRealPortHandler, MODATA_CLONE, 0.0);
    g_portHandlers[i++] = gNew(MoRealPortHandler, MODATA_TIME, 0.0);
    g_portHandlers[i++] = gNew(MoRealPortHandler, MODATA_FALLOFF_WGT, 1.0);
    g_portHandlers[i++] = gNew(MoRealPortHandler, MODATA_GROWTH, 0.0);
}

static void FreeMoPortHandlers() {
    for (LONG i=0; i < g_portHandlerCount; i++) {
        DeleteObj(g_portHandlers[i]);
    }
    DeleteMem(g_portHandlers);
    g_portHandlerCount = 0;
}

MoPortHandler* FindMoPortHandler(LONG dataId) {
    for (LONG i=0; i < g_portHandlerCount; i++) {
        MoPortHandler* handler = g_portHandlers[i];
        if (handler && handler->GetId() == dataId) return handler;
    }
    return NULL;
}


static PortInfo* FindPortInformation(LONG id) {
    for (PortInfo* port=g_ports; port->id != 0; port++) {
        if (port->id == id) return port;
    }
    return NULL;
}

static Bool NodeHasPort(GvNode* node, LONG id, GvPortIO portIo) {
    LONG count = 0;
    if (portIo == GV_PORT_INPUT) count = node->GetInPortCount();
    else if (portIo == GV_PORT_OUTPUT) count = node->GetOutPortCount();
    else return 0;

    for (LONG i=0; i < count; i++) {
        GvPort* port = NULL;
        if (portIo == GV_PORT_INPUT) port = node->GetInPort(i);
        else if (portIo == GV_PORT_OUTPUT) port = node->GetOutPort(i);
        if (port && port->GetMainID() == id) return TRUE;
    }

    return FALSE;
}

static GeData GetPortGeData(GvNode* node, GvPort* port, GvRun* run, Bool* success) {
    if (success) *success = FALSE;
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
            if (success) *success = TRUE;
        }
    }
    GvFreeDynamicData(data);
    return result;
}

static Bool SetPortGeData(const GeData& ge_data, GvNode* node, GvPort* port, GvRun* run) {
    if (!node || !port || !run) return FALSE;

    GvPortDescription pd;
    if (!node->GetPortDescription(port->GetIO(), port->GetMainID(), &pd)) return FALSE;

    GvDataInfo* info = GvGetWorld()->GetDataTypeInfo(pd.data_id);
    if (!info) return FALSE;

    GvDynamicData data;
    GvAllocDynamicData(node, data, info);

    CUSTOMDATATYPEPLUGIN* pl = FindCustomDataTypePlugin(data.info->data_handler->data_id);
    Bool ok = TRUE;
    ok = ok && pl;
    ok = ok && CallCustomDataType(pl, ConvertGeDataToGv)(ge_data, data.data, 0);
    ok = ok && port->SetData(data.data, data.info->value_handler->value_id, run);
    GvFreeDynamicData(data);
    return ok;
}


class MoDataNodeData : public GvOperatorData {

    typedef GvOperatorData super;

public:

    static NodeData* Alloc() { return gNew(MoDataNodeData); }

    MoDataNodeData() : super() { }

    virtual ~MoDataNodeData() { }

    //| GvOperatorData Overrides

    virtual Bool iGetPortDescription(GvNode* node, GvPortIO portIo, LONG id, GvPortDescription* pd) {
        if (!node || !pd) return NULL;
        Bool easyMode = GetEasyMode(node);

        PortInfo* p = FindPortInformation(id);
        if (p && p->CheckPortIOMode(portIo, easyMode)) {
            pd->name = p->GetName();
            pd->flags = p->descflags;
            pd->data_id = p->dtype;
            return TRUE;
        }
        return FALSE;
    }

    virtual Bool iCreateOperator(GvNode* node) {
        if (!node) return FALSE;

        node->SetParameter(MODATANODE_EASYMODE, GeData(TRUE), DESCFLAGS_SET_0);

        Bool ok = TRUE;
        ok = ok && node->AddPort(GV_PORT_INPUT, MODATANODE_INDEX) != NULL;
        ok = ok && node->AddPort(GV_PORT_OUTPUT, MODATANODE_HOST) != NULL;
        ok = ok && node->AddPort(GV_PORT_OUTPUT, MODATANODE_OBJECT) != NULL;
        ok = ok && node->AddPort(GV_PORT_OUTPUT, MODATANODE_COUNT) != NULL;
        return ok;
    }

    virtual LONG FillPortsMenu(GvNode* node, BaseContainer& names, BaseContainer& ids,
            GvValueID valueType, GvPortIO portIo, LONG firstMenuId) {
        if (!node) return FALSE;
        switch (portIo) {
            case GV_PORT_INPUT:
            case GV_PORT_OUTPUT:
                break; // continue with execution
            case GV_PORT_INVALID:
            case GV_PORT_INPUT_OR_GEDATA:
            default:
                return TRUE; // TODO: unkown type, dunno how to handle
        }

        // Check if the node is in easy-mode.
        Bool easyMode = GetEasyMode(node);

        LONG itemCount = 0;
        for (PortInfo* port=g_ports; port->id != 0; port++) {
            if (!port->CheckPortIOMode(portIo, easyMode)) continue;

            String name = port->GetName();
            if (NodeHasPort(node, port->id, portIo)) name += "&d&";

            names.SetString(firstMenuId, name);
            ids.SetLong(firstMenuId, port->id);
            firstMenuId++;
            itemCount++;
        }

        return itemCount;
    }

    virtual Bool AddToCalculationTable(GvNode* node, GvRun* run) {
        if (!node || !run || !m_execInfo.md) return FALSE;
        return run->AddNodeToCalculationTable(node);
    }

    virtual Bool InitCalculation(GvNode* node, GvCalc* calc, GvRun* run) {
        if (!node || !calc || !run) return FALSE;

        // Retrieve the Execution-Info.
        m_execInfo.Clean();
        XPressoEffector* effector = GetNodeXPressoEffector(node);
        if (effector) {
            effector->GetExecInfo(&m_execInfo);
        }
        else {
            return FALSE;
        }

        if (m_execInfo.easyOn) {
            *m_execInfo.easyOn = GetEasyMode(node);
        }

        if (!GvBuildValuesTable(node, m_values, calc, run, GV_EXISTING_PORTS)) {
            return FALSE;
        }
        return super::InitCalculation(node, calc, run);
    }

    virtual void FreeCalculation(GvNode* node, GvCalc* calc) {
        GvFreeValuesTable(node, m_values);
        super::FreeCalculation(node, calc);
    }

    virtual Bool Calculate(GvNode* node, GvPort* outPort, GvRun* run, GvCalc* calc) {
        if (!node || !run || !calc || !m_execInfo.md) return FALSE;
        if (!GvCalculateInValuesTable(node, run, calc, m_values)) {
            return FALSE;
        }

        MoData* md = m_execInfo.md;
        BaseObject* gen = m_execInfo.gen;
        C4D_Falloff* falloff = m_execInfo.falloff;
        BaseSelect* sel = m_execInfo.sel;
        XPressoEffector* effector = GetNodeXPressoEffector(node);

        LONG cloneIndex;
        GvPort* indexPort = node->GetInPortFirstMainID(MODATANODE_INDEX);
        if (indexPort && indexPort->GetInteger(&cloneIndex, run)) ;
        else cloneIndex = 0;

        // Check if the index is in range.
        LONG cloneCount = md->GetCount();
        if (cloneCount <= 0) {
            return FALSE;
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
            for (LONG i=0; i < m_values.nr_of_in_values; i++) {
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
            return TRUE;
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
                    Real value = 1.0;
                    if (falloff) {
                        MDArray<Matrix> matArray = md->GetMatrixArray(MODATA_MATRIX);
                        MDArray<Real> weightArray = md->GetRealArray(MODATA_WEIGHT);

                        Matrix mat = gen->GetMg() * (matArray ? matArray[cloneIndex] : Matrix());
                        falloff->Sample(mat.off, &value, TRUE, 0.0);
                    }
                    return outPort->SetReal(value, run);
                }
                case MODATANODE_SELWEIGHT: {
                    Bool value = TRUE;
                    if (sel) {
                        value = sel->IsSelected(cloneIndex);
                    }
                    return outPort->SetBool(value, run);
                }
                default:
                    if (!ServeDynamicOutport(node, outPort, run, md, cloneIndex)) {
                        GePrint("Could not serve out-port " + LongToString(outPort->GetMainID()));
                        return FALSE;
                    }
                    return TRUE;
            }
        }

        return FALSE;
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
        Bool easyMode = gEasyMode.GetLong();
        return easyMode;
    }

    Bool ServeDynamicInport(GvNode* node, GvPort* port, GvRun* run, MoData* md, LONG index) {
        LONG mainId = port->GetMainID();
        LONG handlerId = MoDataIdToHandlerId(mainId);

        // If we are in easy mode, we want to use the weight-sub ports.
        if (GetEasyMode(node) && m_execInfo.easyValues && mainId == MODATANODE_EASYWEIGHT) {
            Vector* value = m_execInfo.easyValues + index;
            return port->GetVector(value, run);
        }

        MoPortHandler* handler = FindMoPortHandler(handlerId);
        if (!handler) {
            GePrint(__FUNCTION__ ": No MoPortHandler found for ID " + LongToString(handlerId));
            return FALSE;
        }

        switch (mainId) {
            case MODATA_SIZE: {
                Matrix* matrices = ((AwesomeMoPortHandler<Matrix>*) handler)->EnsureArray(md);
                if (matrices) {
                    Matrix& mat = matrices[index];
                    Vector size;
                    if (!port->GetVector(&size, run)) return FALSE;
                    mat.v1 = mat.v1.GetNormalized() * size.x;
                    mat.v2 = mat.v2.GetNormalized() * size.y;
                    mat.v3 = mat.v3.GetNormalized() * size.z;
                    return TRUE;
                }
                break;
            }
            default:
                return handler->PortToMD(port, run, md, index);
        }
        return FALSE;
    }

    Bool ServeDynamicOutport(GvNode* node, GvPort* port, GvRun* run, MoData* md, LONG index) {
        LONG mainId = port->GetMainID();
        LONG handlerId = MoDataIdToHandlerId(mainId);

        MoPortHandler* handler = FindMoPortHandler(handlerId);
        if (!handler) {
            GePrint(__FUNCTION__ ": No MoPortHandler found for ID " + LongToString(handlerId));
            return FALSE;
        }

        switch (mainId) {
            case MODATA_SIZE: {
                Matrix* matrices = ((AwesomeMoPortHandler<Matrix>*) handler)->EnsureArray(md);
                if (matrices) {
                    Matrix& mat = matrices[index];
                    Vector size(
                            mat.v1.GetLength(),
                            mat.v2.GetLength(),
                            mat.v3.GetLength());
                    return port->SetVector(size, run);
                }
                break;
            }
            default:
                return handler->MDToPort(port, run, md, index);
        }
        return FALSE;
    }

    LONG MoDataIdToHandlerId(LONG mainId) {
        // Pre-process the main-id, mapping it to the handler-id.
        LONG handlerId;
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
    nr::c4d::cleanup([] {
        FreeMoPortHandlers();
    });
    return GvRegisterOperatorPlugin(
            GvOperatorID(ID_MODATANODE),
            GeLoadString(IDC_MODATANODE_NAME),
            0,
            MoDataNodeData::Alloc,
            "Gvmodata",
            MODATANODE_VERSION,
            ID_GV_OPCLASS_TYPE_GENERAL,
            ID_GV_OPGROUP_TYPE_XPRESSOEFFECTOR,
            0, // owner
            raii::AutoBitmap("icons/Gvmodata.tif"));
}
