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
 */

#include <c4d_apibridge.h>
#include <c4d_operatordata.h>
#include "lib_csv.h"
#include "CSVNode.h"
#include "internal.xpe_group.h"

#include "res/c4d_symbols.h"
#include "res/description/Gvcsv.h"
#include "misc/raii.h"

#include <NiklasRosenstein/macros.h>

#define CSVNODE_VERSION 1000

using c4d_apibridge::IsEmpty;

GeData GvGetPortGeData(GvNode* node, GvPort* port, GvRun* run, Bool* success=nullptr);
Bool GvSetPortGeData(const GeData& ge_data, GvNode* node, GvPort* port, GvRun* run);
Bool NodeHasPort(GvNode* node, Int32 portId, GvPortIO flag);


struct CSVPort {
    Int32 id;
    Bool inport;
    GvPortDescFlags flags;
    Int32 dtype;
};

CSVPort g_csvports[] = {
    { CSVNODE_FILENAME,         true, GV_PORTDESCRIPTION_STATIC, DTYPE_FILENAME },
    { CSVNODE_HEADER,           true, GV_PORTDESCRIPTION_STATIC, DTYPE_BOOL },
    { CSVNODE_DELIMITER,        true, GV_PORTDESCRIPTION_STATIC, DTYPE_LONG },
    { CSVNODE_ROWINDEX,         true, GV_PORTDESCRIPTION_STATIC, DTYPE_LONG },

    // Non-description ports
    { CSVNODE_LOADED,           false, GV_PORTDESCRIPTION_NONE, DTYPE_BOOL },
    { CSVNODE_COLCOUNT_TOTAL,   false, GV_PORTDESCRIPTION_NONE, DTYPE_LONG },
    { CSVNODE_COLCOUNT,         false, GV_PORTDESCRIPTION_NONE, DTYPE_LONG },
    { CSVNODE_ROWCOUNT,         false, GV_PORTDESCRIPTION_NONE, DTYPE_LONG },
    { 0, 0 },
};

class GvPortCalc {

    GvNode* m_node;
    GvValuesInfo* m_info;
    GvRun* m_run;
    GvCalc* m_calc;

public:

    GvPortCalc(GvNode* node, GvValuesInfo* info, GvRun* run, GvCalc* calc)
    : m_node(node), m_info(info), m_run(run), m_calc(calc) { }

    GvPort* GetInPort(Int32 id) {
        #if API_VERSION < 20000
        if (!m_node || !m_info || !m_info->in_values) return nullptr;
        #else
        if (!m_node || !m_info || m_info->in_values.IsEmpty()) return nullptr;
        #endif

        GvValue* value = nullptr;

        #if API_VERSION < 20000
        Int count = m_info->nr_of_in_values;
        #else
        Int count = m_info->in_values.GetCount();
        #endif

        for (Int32 i=0; i < count; i++) {
            if (m_info->in_values[i]->GetMainID() == id) {
                value = m_info->in_values[i];
                break;
            }
        }
        if (!value) return nullptr;

        GvPort* port = nullptr;
        if (value->IsConnected(0) && m_calc) {
            // TODO: Memory leak
            value->Calculate(m_node, GV_PORT_INPUT, m_run, m_calc);
        }
        port = value->GetPort();
        return port;
    }

};


class CSVNodeData : public GvOperatorData {

    typedef GvOperatorData super;

public:

    static NodeData* Alloc() { return NewObjClear(CSVNodeData); }

    CSVNodeData()
    : super(), m_forceUpdate(true), m_pHeader(false), m_pDelimiter(0) { }

    //| GvOperatorData Overrides

    virtual Bool iCreateOperator(GvNode* node);

    virtual Bool iGetPortDescription(GvNode* host, GvPortIO port, Int32 id,
            GvPortDescription* desc);

    virtual Int32 FillPortsMenu(GvNode* node, BaseContainer& names, BaseContainer& ids,
            GvValueID value_type, GvPortIO flags, Int32 firstId);

    virtual Bool InitCalculation(GvNode* node, GvCalc* calc, GvRun* run);

    virtual void FreeCalculation(GvNode* node, GvCalc* calc);

    virtual Bool Calculate(GvNode* node, GvPort* port, GvRun* run, GvCalc* calc);

    //| NodeData Overrides

    virtual Bool Init(GeListNode* node);

    virtual Bool Message(GeListNode* node, Int32 type, void* pData);

    virtual Bool GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data,
            DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);

private:

    void AddPort(GvNode* node, Int32 mainId) const;

    String GetTableColumnPortName(Int32 column) const;

    Bool UpdateCSVTable(GvNode* node, GvRun* run, GvCalc* calc);

    // Full life-cycle members
    StringCSVTable m_table;
    Bool m_forceUpdate;

    // Calculation members
    GvValuesInfo m_values;
    Bool m_calcInit;
    Bool m_initCsv;

    // Previous values to check if an update is required.
    Bool m_pHeader;
    Int32 m_pDelimiter;

};

Bool CSVNodeData::iCreateOperator(GvNode* node) {
    // Add the default in and output ports.
    AddPort(node, CSVNODE_FILENAME);
    AddPort(node, CSVNODE_HEADER);
    AddPort(node, CSVNODE_DELIMITER);
    AddPort(node, CSVNODE_ROWINDEX);
    AddPort(node, CSVNODE_COLCOUNT_TOTAL);
    AddPort(node, CSVNODE_COLCOUNT);
    AddPort(node, CSVNODE_ROWCOUNT);
    return super::iCreateOperator(node);
}

Bool CSVNodeData::iGetPortDescription(GvNode* host, GvPortIO port, Int32 id,
                                      GvPortDescription* desc) {
    Bool result = false;
    for (Int32 i=0; g_csvports[i].id != 0; i++) {
        const CSVPort& port = g_csvports[i];
        if (port.id == id) {
            desc->name = GeLoadString(port.id);
            desc->data_id = port.dtype;
            desc->flags = port.flags;
            result = true;
            break;
        }
    }
    if (!result && id >= CSVNODE_DYNPORT_START && m_table.Loaded()) {
        Int32 index = id - CSVNODE_DYNPORT_START;
        Int32 colCount = m_table.GetColumnCount();
        if (index < colCount) {
            desc->name = GetTableColumnPortName(index);
            desc->data_id = DTYPE_STRING;
            desc->flags = GV_PORTDESCRIPTION_NONE;
            result = true;
        }
        else {
            result = false;
        }
    }
    if (!result) {
        desc->name = GeLoadString(IDC_CSVNODE_INVALIDPORT);
        desc->data_id = DTYPE_NONE;
    }
    return result;
}

Int32 CSVNodeData::FillPortsMenu(GvNode* node, BaseContainer& names, BaseContainer& ids,
            GvValueID value_type, GvPortIO flag, Int32 firstId) {
    // Validate the passed flag.
    switch (flag) {
    case GV_PORT_INPUT:
    case GV_PORT_OUTPUT:
        break;
    case GV_PORT_INVALID:
    case GV_PORT_INPUT_OR_GEDATA:
    default:
        // DEBUG:
        GePrint(String(NR_CURRENT_FUNCTION) + ": Invalid flag passed. (" + String::IntToString(flag) + ")");
        return 0;
    } // end switch

    // Retrieve a list of already available ports.
    AutoAlloc<GvPortList> availPorts;
    if (!availPorts) return 0; // memory error
    node->GetPortList(flag, availPorts);

    // Iterate over all ports available ports and yield them.
    Int32 countAdded = 0;
    for (Int32 i=0; g_csvports[i].id != 0; i++) {
        const CSVPort& port = g_csvports[i];

        // Skip this port if it is not requested (eg. an INPORT, but only
        // OUTPORTs are requested).
        if (port.inport && flag != GV_PORT_INPUT) continue;
        if (!port.inport && flag != GV_PORT_OUTPUT) continue;

        // Check if the port already exists.
        Bool exists = NodeHasPort(node, port.id, flag);

        // Fill in the menu containers.
        String name = GeLoadString(port.id) + String(exists ? "&d&" : "");
        names.SetString(firstId + i, name);
        ids.SetInt32(firstId + i, port.id);
        countAdded++;
    }

    // Add the CSV Table's output ports.
    if (flag == GV_PORT_OUTPUT && m_table.Loaded()) {
        Int32 colCount = m_table.GetColumnCount();
        GeData forceCols, forceColsCount;
        node->GetParameter(CSVNODE_FORCEOUTPORTS, forceCols, DESCFLAGS_GET_0);
        node->GetParameter(CSVNODE_FORCEOUTPORTS_COUNT, forceColsCount, DESCFLAGS_GET_0);
        if (forceCols.GetBool() && forceColsCount.GetInt32() > colCount) {
            colCount = forceColsCount.GetInt32();
        }

        for (Int32 i=0; i < colCount; i++) {
            // Compute the port id and check if the port already exists.
            Int32 portId = CSVNODE_DYNPORT_START + i;
            Bool exists = NodeHasPort(node, portId, flag);

            // Compute the name of the port.
            String name = GetTableColumnPortName(i);
            names.SetString(firstId + portId, name + String(exists ? "&d&" : ""));
            ids.SetInt32(firstId + portId, portId);
            countAdded++;
        }
    }

    return countAdded;
}

Bool CSVNodeData::InitCalculation(GvNode* node, GvCalc* calc, GvRun* run) {
    if (!node || !calc || !run) return false;
    m_calcInit = false;
    m_initCsv = false;

    if (!GvBuildValuesTable(node, m_values, calc, run, GV_EXISTING_PORTS)) {
        GePrint(String(NR_CURRENT_FUNCTION) + ": Could not build in-port values.");
        return false;
    }
    UpdateCSVTable(node, run, nullptr);
    m_calcInit = true;
    return m_calcInit && super::InitCalculation(node, calc, run);
}

void CSVNodeData::FreeCalculation(GvNode* node, GvCalc* calc) {
    GvFreeValuesTable(node, m_values);
    super::FreeCalculation(node, calc);
}

Bool CSVNodeData::Calculate(GvNode* node, GvPort* port, GvRun* run, GvCalc* calc) {
    if (!node || !port || !run || !calc || !m_calcInit) return false;

    // Obtain the row-index from the in-ports.
    GvPortCalc pCalc(node, &m_values, run, calc);
    GvPort* pRow = pCalc.GetInPort(CSVNODE_ROWINDEX); // node->GetInPortFirstMainID(CSVNODE_ROWINDEX);
    Int32 rowIndex;
    if (!pRow || !pRow->GetInteger(&rowIndex, run)) {
        GePrint(String(NR_CURRENT_FUNCTION) + ": Could not obtain Row Index.");
        return false;
    }

    // Update the CSV Table.
    if (!UpdateCSVTable(node, run, calc)) {
        GePrint(String(NR_CURRENT_FUNCTION) + ": UpdateCSVTable() failed.");
        return false;
    }

    // Obtain the row from the CSV Table that is to be used.
    const StringCSVTable::Array* row = nullptr;
    Int32 rowCount = m_table.GetRowCount();
    if (rowIndex >= 0 && rowIndex < rowCount) {
        row = &m_table.GetRow(rowIndex);
    }

    // True when setting the outgoing value for the port was
    // successful, false if not.
    Bool result = true;

    // Obtain the port Main ID and check for fixed ports (like
    // the total column count, row count, etc.)
    Int32 portId = port->GetMainID();
    switch (portId) {
    case CSVNODE_LOADED:
        port->SetBool(m_table.Loaded(), run);
        break;
    case CSVNODE_COLCOUNT_TOTAL:
        port->SetInteger(m_table.GetColumnCount(), run);
        break;
    case CSVNODE_COLCOUNT: {
        Int32 count = row ? row->GetCount() : 0;
        port->SetInteger(count, run);
        break;
    }
    case CSVNODE_ROWCOUNT:
        port->SetInteger(m_table.GetRowCount(), run);
        break;
    default:
        result = false;
        break;
    }

    // If no port matched, it probably is a dynamic port representing
    // a CSV column.
    if (!result && portId >= CSVNODE_DYNPORT_START) {
        Int32 index = portId - CSVNODE_DYNPORT_START;
        Int32 colCount = m_table.GetColumnCount();

        String value = "";
        if (index >= 0 && index < colCount && row) {
            value = (*row)[index];
        }

        port->SetString(value, run);
        result = true;
    }

    return result;
}

Bool CSVNodeData::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return false;

    node->SetParameter(CSVNODE_FORCEOUTPORTS, Bool(false), DESCFLAGS_SET_0);
    node->SetParameter(CSVNODE_FORCEOUTPORTS_COUNT, Int32(0), DESCFLAGS_SET_0);

    // Can't initialize with container, need to use SetParameter.
    node->SetParameter(CSVNODE_FILENAME, Filename(""), DESCFLAGS_SET_0);
    node->SetParameter(CSVNODE_HEADER, true, DESCFLAGS_SET_0);
    node->SetParameter(CSVNODE_DELIMITER, CSVNODE_DELIMITER_COMMA, DESCFLAGS_SET_0);
    node->SetParameter(CSVNODE_ROWINDEX, 0, DESCFLAGS_SET_0);
    return true;
}

Bool CSVNodeData::Message(GeListNode* gNode, Int32 type, void* pData)  {
    if (!gNode) return false;
    GvNode* node = (GvNode*) gNode;
    switch (type) {
    case MSG_DESCRIPTION_POSTSETPARAMETER: {
        DescriptionPostSetValue* data = (DescriptionPostSetValue*) pData;
        if (!data) return false;
        Int32 id = (*data->descid)[data->descid->GetDepth() - 1].id;

        switch (id) {
        case CSVNODE_FILENAME:
        case CSVNODE_HEADER:
        case CSVNODE_DELIMITER:
            m_forceUpdate = true;
            break;
        } // end switch
        break;
    }
    } // end switch
    return super::Message(node, type, pData);
}

Bool CSVNodeData::GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data,
            DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
    if (id == CSVNODE_FORCEOUTPORTS_COUNT) {
        GeData data;
        if (node->GetParameter(CSVNODE_FORCEOUTPORTS, data, DESCFLAGS_GET_0)) {
            return data.GetBool();
        }
    }
    return super::GetDEnabling(node, id, t_data, flags, itemdesc);
}

void CSVNodeData::AddPort(GvNode* node, Int32 mainId) const {
    if (!node) return;
    const CSVPort* port = nullptr;
    for (Int32 i=0; g_csvports[i].id != 0; i++) {
        port = &g_csvports[i];
        if (port->id == mainId) break;
        port = nullptr;
    }
    if (!port) return;

    GvPortIO flag = port->inport ? GV_PORT_INPUT : GV_PORT_OUTPUT;
    if (!NodeHasPort(node, mainId, flag)) {
        node->AddPort(flag, mainId, GV_PORT_FLAG_IS_VISIBLE, true);
    }
}

Bool CSVNodeData::UpdateCSVTable(GvNode* node, GvRun* run, GvCalc* calc) {
    BaseDocument* doc = node->GetDocument();
    if (m_initCsv) return true;

    // Obtain data to update the CSV Table. If *all* of those
    // succeeded, the CSV table is updated.
    GvPortCalc pCalc(node, &m_values, run, calc);
    Bool header;
    Int32 delimiter;
    Filename filename;

    GvPort* pHeader = pCalc.GetInPort(CSVNODE_HEADER);
    if (pHeader && pHeader->GetBool(&header, run)) ;
    else return true;

    GvPort* pDelimiter = pCalc.GetInPort(CSVNODE_DELIMITER);
    if (pDelimiter && pDelimiter->GetInteger(&delimiter, run)) ;
    else return true;

    Bool ok = false;
    GvPort* pFilename = pCalc.GetInPort(CSVNODE_FILENAME);
    filename = GvGetPortGeData(node, pFilename, run, &ok).GetFilename();
    if (!ok) return true;

    // We now initialize the CSV Table and want to prevent any
    // further initialization in the calculation cycle.
    m_initCsv = true;

    // Validate and set the data to the CSV Table object.
    if (m_pHeader != header) {
        m_forceUpdate = true;
    }
    m_pHeader = header;
    m_table.SetHasHeader(header);

    if (delimiter < 0 || delimiter > 255) {
        delimiter = CSVNODE_DELIMITER_COMMA;
    }
    if (m_pDelimiter != delimiter) {
        m_forceUpdate = true;
    }
    m_pDelimiter = delimiter;
    m_table.SetDelimiter((Char) delimiter);

    // Make the CSV Filename relative to the node's document if the file
    // does not exist.
    if (doc && !IsEmpty(filename) && !GeFExist(filename)) {
        filename = doc->GetDocumentPath() + filename;
    }

    // Update the CSV Table.
    Bool updated = false;
    Bool success = m_table.Init(filename, m_forceUpdate, &updated);
    m_forceUpdate = false;

    return success;
}

String CSVNodeData::GetTableColumnPortName(Int32 column) const {
    if (!m_table.Loaded() || column < 0) {
        return GeLoadString(IDC_CSVNODE_INVALIDPORT);
    }

    String prefix = GeLoadString(IDC_CSVNODE_TABLECOLUMN_PREFIX);
    const CSVRow& header = m_table.GetHeader();

    if (column < header.GetCount()) {
        return prefix + header[column];
    }
    else {
        return prefix + String::IntToString(column + 1);
    }
}


GeData GvGetPortGeData(GvNode* node, GvPort* port, GvRun* run, Bool* success) {
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

Bool GvSetPortGeData(const GeData& ge_data, GvNode* node, GvPort* port, GvRun* run) {
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

Bool NodeHasPort(GvNode* node, Int32 portId, GvPortIO flag) {
    Bool exists = false;
    Int32 count = 0;
    if (flag == GV_PORT_INPUT) count = node->GetInPortCount();
    else if (flag == GV_PORT_OUTPUT) count = node->GetOutPortCount();
    for (Int32 j=0; j < count; j++) {
        GvPort* gvport = nullptr;
        if (flag == GV_PORT_INPUT) gvport = node->GetInPort(j);
        else if (flag == GV_PORT_OUTPUT) gvport = node->GetOutPort(j);
        if (gvport && gvport->GetMainID() == portId) {
            exists = true;
            break;
        }
    }
    return exists;
}


Bool RegisterCSVNode() {
    return GvRegisterOperatorPlugin(
        GvOperatorID(ID_CSVNODE),
        GeLoadString(IDC_CSVNODE_NAME),
        0,
        CSVNodeData::Alloc,
        "Gvcsv"_s,
        CSVNODE_VERSION,
        ID_GV_OPCLASS_TYPE_GENERAL,
        ID_GV_OPGROUP_TYPE_XPRESSOEFFECTOR,
        0, // owner
        raii::AutoBitmap("icons/Gvcsv.tif"_s));
}


