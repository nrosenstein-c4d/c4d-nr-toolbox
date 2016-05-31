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

#include "base.h"

#include "misc/legacy.h"
#include <c4d_operatordata.h>
#include "lib_csv.h"
#include "CSVNode.h"
#include "internal.xpe_group.h"

#include "res/c4d_symbols.h"
#include "res/description/Gvcsv.h"
#include "misc/raii.h"

#define CSVNODE_VERSION 1000


GeData GvGetPortGeData(GvNode* node, GvPort* port, GvRun* run, Bool* success=NULL);
Bool GvSetPortGeData(const GeData& ge_data, GvNode* node, GvPort* port, GvRun* run);
Bool NodeHasPort(GvNode* node, LONG portId, GvPortIO flag);


struct CSVPort {
    LONG id;
    Bool inport;
    GvPortDescFlags flags;
    LONG dtype;
};

CSVPort g_csvports[] = {
    { CSVNODE_FILENAME,         TRUE, GV_PORTDESCRIPTION_STATIC, DTYPE_FILENAME },
    { CSVNODE_HEADER,           TRUE, GV_PORTDESCRIPTION_STATIC, DTYPE_BOOL },
    { CSVNODE_DELIMITER,        TRUE, GV_PORTDESCRIPTION_STATIC, DTYPE_LONG },
    { CSVNODE_ROWINDEX,         TRUE, GV_PORTDESCRIPTION_STATIC, DTYPE_LONG },

    // Non-description ports
    { CSVNODE_LOADED,           FALSE, GV_PORTDESCRIPTION_NONE, DTYPE_BOOL },
    { CSVNODE_COLCOUNT_TOTAL,   FALSE, GV_PORTDESCRIPTION_NONE, DTYPE_LONG },
    { CSVNODE_COLCOUNT,         FALSE, GV_PORTDESCRIPTION_NONE, DTYPE_LONG },
    { CSVNODE_ROWCOUNT,         FALSE, GV_PORTDESCRIPTION_NONE, DTYPE_LONG },
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

    GvPort* GetInPort(LONG id) {
        if (!m_node || !m_info || !m_info->in_values) return FALSE;
        GvValue* value = NULL;
        for (LONG i=0; i < m_info->nr_of_in_values; i++) {
            if (m_info->in_values[i]->GetMainID() == id) {
                value = m_info->in_values[i];
                break;
            }
        }
        if (!value) return FALSE;

        GvPort* port = NULL;
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

    static NodeData* Alloc() { return gNew(CSVNodeData); }

    CSVNodeData()
    : super(), m_forceUpdate(TRUE), m_pHeader(FALSE), m_pDelimiter(0) { }

    //| GvOperatorData Overrides

    virtual Bool iCreateOperator(GvNode* node);

    virtual Bool iGetPortDescription(GvNode* host, GvPortIO port, LONG id,
            GvPortDescription* desc);

    virtual LONG FillPortsMenu(GvNode* node, BaseContainer& names, BaseContainer& ids,
            GvValueID value_type, GvPortIO flags, LONG firstId);

    virtual Bool InitCalculation(GvNode* node, GvCalc* calc, GvRun* run);

    virtual void FreeCalculation(GvNode* node, GvCalc* calc);

    virtual Bool Calculate(GvNode* node, GvPort* port, GvRun* run, GvCalc* calc);

    //| NodeData Overrides

    virtual Bool Init(GeListNode* node);

    virtual Bool Message(GeListNode* node, LONG type, void* pData);

    virtual Bool GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data,
            DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);

private:

    void AddPort(GvNode* node, LONG mainId) const;

    String GetTableColumnPortName(LONG column) const;

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
    LONG m_pDelimiter;

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

Bool CSVNodeData::iGetPortDescription(GvNode* host, GvPortIO port, LONG id,
                                      GvPortDescription* desc) {
    Bool result = FALSE;
    for (LONG i=0; g_csvports[i].id != 0; i++) {
        const CSVPort& port = g_csvports[i];
        if (port.id == id) {
            desc->name = GeLoadString(port.id);
            desc->data_id = port.dtype;
            desc->flags = port.flags;
            result = TRUE;
            break;
        }
    }
    if (!result && id >= CSVNODE_DYNPORT_START && m_table.Loaded()) {
        LONG index = id - CSVNODE_DYNPORT_START;
        LONG colCount = m_table.GetColumnCount();
        if (index < colCount) {
            desc->name = GetTableColumnPortName(index);
            desc->data_id = DTYPE_STRING;
            desc->flags = GV_PORTDESCRIPTION_NONE;
            result = TRUE;
        }
        else {
            result = FALSE;
        }
    }
    if (!result) {
        desc->name = GeLoadString(IDC_CSVNODE_INVALIDPORT);
        desc->data_id = DTYPE_NONE;
    }
    return result;
}

LONG CSVNodeData::FillPortsMenu(GvNode* node, BaseContainer& names, BaseContainer& ids,
            GvValueID value_type, GvPortIO flag, LONG firstId) {
    // Validate the passed flag.
    switch (flag) {
    case GV_PORT_INPUT:
    case GV_PORT_OUTPUT:
        break;
    case GV_PORT_INVALID:
    case GV_PORT_INPUT_OR_GEDATA:
    default:
        // DEBUG:
        GePrint(__FUNCTION__ ": Invalid flag passed. (" + LongToString(flag) + ")");
        return 0;
    } // end switch

    // Retrieve a list of already available ports.
    AutoAlloc<GvPortList> availPorts;
    if (!availPorts) return 0; // memory error
    node->GetPortList(flag, availPorts);

    // Iterate over all ports available ports and yield them.
    LONG countAdded = 0;
    for (LONG i=0; g_csvports[i].id != 0; i++) {
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
        ids.SetLong(firstId + i, port.id);
        countAdded++;
    }

    // Add the CSV Table's output ports.
    if (flag == GV_PORT_OUTPUT && m_table.Loaded()) {
        LONG colCount = m_table.GetColumnCount();
        GeData forceCols, forceColsCount;
        node->GetParameter(CSVNODE_FORCEOUTPORTS, forceCols, DESCFLAGS_GET_0);
        node->GetParameter(CSVNODE_FORCEOUTPORTS_COUNT, forceColsCount, DESCFLAGS_GET_0);
        if (forceCols.GetBool() && forceColsCount.GetLong() > colCount) {
            colCount = forceColsCount.GetLong();
        }

        for (LONG i=0; i < colCount; i++) {
            // Compute the port id and check if the port already exists.
            LONG portId = CSVNODE_DYNPORT_START + i;
            Bool exists = NodeHasPort(node, portId, flag);

            // Compute the name of the port.
            String name = GetTableColumnPortName(i);
            names.SetString(firstId + portId, name + String(exists ? "&d&" : ""));
            ids.SetLong(firstId + portId, portId);
            countAdded++;
        }
    }

    return countAdded;
}

Bool CSVNodeData::InitCalculation(GvNode* node, GvCalc* calc, GvRun* run) {
    if (!node || !calc || !run) return FALSE;
    m_calcInit = FALSE;
    m_initCsv = FALSE;

    if (!GvBuildValuesTable(node, m_values, calc, run, GV_EXISTING_PORTS)) {
        GePrint(__FUNCTION__ ": Could not build in-port values.");
        return FALSE;
    }
    UpdateCSVTable(node, run, NULL);
    m_calcInit = TRUE;
    return m_calcInit && super::InitCalculation(node, calc, run);
}

void CSVNodeData::FreeCalculation(GvNode* node, GvCalc* calc) {
    GvFreeValuesTable(node, m_values);
    super::FreeCalculation(node, calc);
}

Bool CSVNodeData::Calculate(GvNode* node, GvPort* port, GvRun* run, GvCalc* calc) {
    if (!node || !port || !run || !calc || !m_calcInit) return FALSE;

    // Obtain the row-index from the in-ports.
    GvPortCalc pCalc(node, &m_values, run, calc);
    GvPort* pRow = pCalc.GetInPort(CSVNODE_ROWINDEX); // node->GetInPortFirstMainID(CSVNODE_ROWINDEX);
    LONG rowIndex;
    if (!pRow || !pRow->GetInteger(&rowIndex, run)) {
        GePrint(__FUNCTION__ ": Could not obtain Row Index.");
        return FALSE;
    }

    // Update the CSV Table.
    if (!UpdateCSVTable(node, run, calc)) {
        GePrint(__FUNCTION__ ": UpdateCSVTable() failed.");
        return FALSE;
    }

    // Obtain the row from the CSV Table that is to be used.
    const StringCSVTable::Array* row = NULL;
    LONG rowCount = m_table.GetRowCount();
    if (rowIndex >= 0 && rowIndex < rowCount) {
        row = &m_table.GetRow(rowIndex);
    }

    // True when setting the outgoing value for the port was
    // successful, false if not.
    Bool result = TRUE;

    // Obtain the port Main ID and check for fixed ports (like
    // the total column count, row count, etc.)
    LONG portId = port->GetMainID();
    switch (portId) {
    case CSVNODE_LOADED:
        port->SetBool(m_table.Loaded(), run);
        break;
    case CSVNODE_COLCOUNT_TOTAL:
        port->SetInteger(m_table.GetColumnCount(), run);
        break;
    case CSVNODE_COLCOUNT: {
        LONG count = row ? row->GetCount() : 0;
        port->SetInteger(count, run);
        break;
    }
    case CSVNODE_ROWCOUNT:
        port->SetInteger(m_table.GetRowCount(), run);
        break;
    default:
        result = FALSE;
        break;
    }

    // If no port matched, it probably is a dynamic port representing
    // a CSV column.
    if (!result && portId >= CSVNODE_DYNPORT_START) {
        LONG index = portId - CSVNODE_DYNPORT_START;
        LONG colCount = m_table.GetColumnCount();

        String value = "";
        if (index >= 0 && index < colCount && row) {
            value = (*row)[index];
        }

        port->SetString(value, run);
        result = TRUE;
    }

    return result;
}

Bool CSVNodeData::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return FALSE;

    node->SetParameter(CSVNODE_FORCEOUTPORTS, Bool(FALSE), DESCFLAGS_SET_0);
    node->SetParameter(CSVNODE_FORCEOUTPORTS_COUNT, LONG(0), DESCFLAGS_SET_0);

    // Can't initialize with container, need to use SetParameter.
    node->SetParameter(CSVNODE_FILENAME, Filename(""), DESCFLAGS_SET_0);
    node->SetParameter(CSVNODE_HEADER, TRUE, DESCFLAGS_SET_0);
    node->SetParameter(CSVNODE_DELIMITER, CSVNODE_DELIMITER_COMMA, DESCFLAGS_SET_0);
    node->SetParameter(CSVNODE_ROWINDEX, 0, DESCFLAGS_SET_0);
    return TRUE;
}

Bool CSVNodeData::Message(GeListNode* gNode, LONG type, void* pData)  {
    if (!gNode) return FALSE;
    GvNode* node = (GvNode*) gNode;
    switch (type) {
    case MSG_DESCRIPTION_POSTSETPARAMETER: {
        DescriptionPostSetValue* data = (DescriptionPostSetValue*) pData;
        if (!data) return FALSE;
        LONG id = (*data->descid)[data->descid->GetDepth() - 1].id;

        switch (id) {
        case CSVNODE_FILENAME:
        case CSVNODE_HEADER:
        case CSVNODE_DELIMITER:
            m_forceUpdate = TRUE;
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

void CSVNodeData::AddPort(GvNode* node, LONG mainId) const {
    if (!node) return;
    const CSVPort* port = NULL;
    for (LONG i=0; g_csvports[i].id != 0; i++) {
        port = &g_csvports[i];
        if (port->id == mainId) break;
        port = NULL;
    }
    if (!port) return;

    GvPortIO flag = port->inport ? GV_PORT_INPUT : GV_PORT_OUTPUT;
    if (!NodeHasPort(node, mainId, flag)) {
        node->AddPort(flag, mainId, GV_PORT_FLAG_IS_VISIBLE, TRUE);
    }
}

Bool CSVNodeData::UpdateCSVTable(GvNode* node, GvRun* run, GvCalc* calc) {
    BaseDocument* doc = node->GetDocument();
    if (m_initCsv) return TRUE;

    // Obtain data to update the CSV Table. If *all* of those
    // succeeded, the CSV table is updated.
    GvPortCalc pCalc(node, &m_values, run, calc);
    Bool header;
    LONG delimiter;
    Filename filename;

    GvPort* pHeader = pCalc.GetInPort(CSVNODE_HEADER);
    if (pHeader && pHeader->GetBool(&header, run)) ;
    else return TRUE;

    GvPort* pDelimiter = pCalc.GetInPort(CSVNODE_DELIMITER);
    if (pDelimiter && pDelimiter->GetInteger(&delimiter, run)) ;
    else return TRUE;

    Bool ok = FALSE;
    GvPort* pFilename = pCalc.GetInPort(CSVNODE_FILENAME);
    filename = GvGetPortGeData(node, pFilename, run, &ok).GetFilename();
    if (!ok) return TRUE;

    // We now initialize the CSV Table and want to prevent any
    // further initialization in the calculation cycle.
    m_initCsv = TRUE;

    // Validate and set the data to the CSV Table object.
    if (m_pHeader != header) {
        m_forceUpdate = TRUE;
    }
    m_pHeader = header;
    m_table.SetHasHeader(header);

    if (delimiter < 0 || delimiter > 255) {
        delimiter = CSVNODE_DELIMITER_COMMA;
    }
    if (m_pDelimiter != delimiter) {
        m_forceUpdate = TRUE;
    }
    m_pDelimiter = delimiter;
    m_table.SetDelimiter((CHAR) delimiter);

    // Make the CSV Filename relative to the node's document if the file
    // does not exist.
    if (doc && filename.Content() && !GeFExist(filename)) {
        filename = doc->GetDocumentPath() + filename;
    }

    // Update the CSV Table.
    Bool updated = FALSE;
    Bool success = m_table.Init(filename, m_forceUpdate, &updated);
    m_forceUpdate = FALSE;

    return success;
}

String CSVNodeData::GetTableColumnPortName(LONG column) const {
    if (!m_table.Loaded() || column < 0) {
        return GeLoadString(IDC_CSVNODE_INVALIDPORT);
    }

    String prefix = GeLoadString(IDC_CSVNODE_TABLECOLUMN_PREFIX);
    const CSVRow& header = m_table.GetHeader();

    if (column < header.GetCount()) {
        return prefix + header[column];
    }
    else {
        return prefix + LongToString(column + 1);
    }
}


GeData GvGetPortGeData(GvNode* node, GvPort* port, GvRun* run, Bool* success) {
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

Bool GvSetPortGeData(const GeData& ge_data, GvNode* node, GvPort* port, GvRun* run) {
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

Bool NodeHasPort(GvNode* node, LONG portId, GvPortIO flag) {
    Bool exists = FALSE;
    LONG count = 0;
    if (flag == GV_PORT_INPUT) count = node->GetInPortCount();
    else if (flag == GV_PORT_OUTPUT) count = node->GetOutPortCount();
    for (LONG j=0; j < count; j++) {
        GvPort* gvport = NULL;
        if (flag == GV_PORT_INPUT) gvport = node->GetInPort(j);
        else if (flag == GV_PORT_OUTPUT) gvport = node->GetOutPort(j);
        if (gvport && gvport->GetMainID() == portId) {
            exists = TRUE;
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
            "Gvcsv",
            CSVNODE_VERSION,
            ID_GV_OPCLASS_TYPE_GENERAL,
            ID_GV_OPGROUP_TYPE_XPRESSOEFFECTOR,
            0, // owner
            raii::AutoBitmap("icons/Gvcsv.tif"));
}


