/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include "XGraphView.h"
#include "res/c4d_symbols.h"
#include "res/description/Gvgraph.h"
#include "NiklasRosenstein/c4d/raii.hpp"
#include <c4d_operatordata.h>
#include <c4d_apibridge.h>

#include <vector>
#include <cassert>

#define ID_XGRAPH_PORT_START 1000

using niklasrosenstein::c4d::auto_bitmap;

class XRealGraph : public XGraphData {

    std::vector<Float> records;
    Int32 record_count;
    Int32 filled_records;
    Int32 last_record_index;

    Float y_min;
    Float y_max;

public:

    Int32 port_id;

    XRealGraph(Int32 port_id=0) : port_id(port_id), records(NULL), record_count(0),
            filled_records(0), last_record_index(-1), y_min(0), y_max(0) {
    }

    virtual ~XRealGraph() {
    }

    void SetCount(Int32 count) {
        records.resize(count);
        record_count = count;
        if (filled_records > count) {
            filled_records = count;
        }
    }

    void StoreRecord(Int32 frame, Float value) {
        if (frame < 0 || frame >= record_count) return;
        records[frame] = value;
        last_record_index = frame;
        if (frame > filled_records) filled_records = frame;

        if (value < y_min) y_min = value;
        if (value > y_max) y_max = value;
    }

    //////// XGraphData

    void GetRange(Float* x_min, Float* x_max, Float* y_min, Float* y_max) const;

    Float GetValue(Float x, XGraphValueState* state) const;

};

void XRealGraph::GetRange(Float* x_min, Float* x_max, Float* y_min, Float* y_max) const {
    *x_min = 0;
    *x_max = record_count;

    if (record_count > 0) {
        *y_min = *y_max = records[0];
        for (Int32 i=1; i < filled_records; i++) {
            Float x = records[i];
            if (x < *y_min) *y_min = x;
            if (x > *y_max) *y_max = x;
        }
    }
    else {
        *y_min = this->y_min;
        *y_max = this->y_max;
    }
}

Float XRealGraph::GetValue(Float x, XGraphValueState* state) const {
    if (x <= record_count - 1 && x >= 0) {
        *state |= XGraphValueState_Valid;

        Int32 left = (Int32) x;
        Int32 right = (Int32) (x + 1);
        if (right >= record_count) return -1;

        assert(right < record_count);

        if (left > last_record_index) {
            *state |= XGraphValueState_Outdated;
        }

        Float v_left = records[left];
        Float v_right = records[right];
        Float mix = fmod(x, 1.0);
        Float y = v_left * (1 - mix) + v_right * mix;
        return y;
    }
    return -1;
}


class XGraphOperatorData : public GvOperatorData {

    typedef GvOperatorData super;

    XGraphView view;
    XGraphDisplay display;
    std::vector<XRealGraph> graphs;

public:

    static NodeData* Alloc() { return NewObjClear(XGraphOperatorData); }

    XGraphOperatorData();

    //////// GvOperatorData

    virtual void EditorDraw(GvNode* host, GvNodeGUI* gui, GeUserArea* area,
            Int32 x1, Int32 y1, Int32 x2, Int32 y2);

    virtual Int32 FillPortsMenu(GvNode* host, BaseContainer& names, BaseContainer& ids,
            GvValueID value_type, GvPortIO port, Int32 first_menu_id);

    virtual Bool iGetPortDescription(GvNode* host, GvPortIO port, Int32 id,
            GvPortDescription* desc);

    virtual void GetBodySize(GvNode* host, Int32* width, Int32* height);

    virtual Bool Calculate(GvNode* host, GvPort* port, GvRun* run, GvCalc* calc);

    virtual Bool AddToCalculationTable(GvNode* host, GvRun* run);

};

XGraphOperatorData::XGraphOperatorData() {
    display.graph_color_outdated = COLOR_BG_DARK1;
    display.axis_color = COLOR_BGEDIT;
    display.text_color = COLOR_CTIMELINE_TEXTCOLOR;
}

void XGraphOperatorData::EditorDraw(GvNode* host, GvNodeGUI* gui, GeUserArea* area,
            Int32 x1, Int32 y1, Int32 x2, Int32 y2) {
    area->DrawSetPen(COLOR_BG);
    area->DrawRectangle(x1, y1, x2, y2);

    XGraphRect rect(x1, y1, x2, y2);
    view.DrawGraphBegin(area, rect, display);
    for (Int32 i=0; i < graphs.size(); i++) {
        XRealGraph* graph = &graphs[i];
        view.DrawGraph(graph, COLOR_CTIMELINE_GREY);
    }
    view.DrawGraphEnd();
}

Int32 XGraphOperatorData::FillPortsMenu(GvNode* host, BaseContainer& names,
            BaseContainer& ids, GvValueID value_type, GvPortIO port,
            Int32 first_menu_id) {
    Int32 id = first_menu_id;

    if (port == GV_PORT_INPUT) {
        Int32 count = host->GetInPortCount();
        Int32 port_id = ID_XGRAPH_PORT_START;

        // Check for the next free id.
        while (TRUE) {
            Bool found = FALSE;

            // Check if the port ID already exists.
            for (Int32 i=0; i < count; i++) {
                GvPort* port = host->GetInPort(i);
                Int32 id = port->GetMainID();
                if (id == port_id) {
                    found = TRUE;
                    break;
                }
            }

            if (found) {
                port_id++;
            }
            else {
                break;
            }
        }

        names.SetString(id, GeLoadString(IDS_GVPORT_NAME_REAL));
        ids.SetInt32(id, port_id);
        id++;
    }

    return id - first_menu_id;
}

Bool XGraphOperatorData::iGetPortDescription(GvNode* host, GvPortIO port, Int32 id,
        GvPortDescription* desc) {
    desc->data_id = DTYPE_REAL;
    desc->flags = GV_PORTDESCRIPTION_NONE;
    return TRUE;
}

void XGraphOperatorData::GetBodySize(GvNode* host, Int32* width, Int32* height) {
    *width = 180;
    *height = 80;
}

Bool XGraphOperatorData::Calculate(GvNode* host, GvPort* port, GvRun* run, GvCalc* calc) {
    BaseDocument* doc = calc->document;
    BaseTime t = doc->GetTime();
    Int32 fps = doc->GetFps();
    Int32 mintime = doc->GetMinTime().GetFrame(fps);
    Int32 maxtime = doc->GetMaxTime().GetFrame(fps);
    Int32 frame = t.GetFrame(fps);

    Int32 count = host->GetInPortCount();
    graphs.resize(count);

    for (Int32 i=0; i < count; i++) {
        XRealGraph* graph = &graphs[i];
        graph->SetCount(maxtime - mintime);

        GvPort* port = host->GetInPort(i);
        if (!port) {
            continue;
        }
        graph->port_id = port->GetMainID();
        GvValue* calcer = host->AllocCalculationHandler(graph->port_id, calc, run, 0);

        // Doesn't work for multiple nodes.
        if (!calcer->Calculate(host, GV_PORT_INPUT, run, calc)) {
            GeDebugOut("GvValue::Calculate() failed for port %d.", graph->port_id);
        }
        Float v;
        if (port->GetFloat(&v, run)) {
            graph->StoreRecord(frame, v);
            run->IncrementID();
        }
        else {
            GeDebugOut("!!! Port %d: value could not be retrieved..", graph->port_id);
        }
        host->FreeCalculationHandler(calcer);
    }

    return TRUE;
}

Bool XGraphOperatorData::AddToCalculationTable(GvNode* host, GvRun* run) {
    return run->AddNodeToCalculationTable(host);
}

Bool RegisterXGraphPlugin() {
    const static Int32 disklevel = 0;
    return GvRegisterOperatorPlugin(
        (GvOperatorID) Gvgraph,
        GeLoadString(IDS_GVOPERATOR_GRAPH),
        GV_OPERATORFLAG_MP_SAVE,
        XGraphOperatorData::Alloc,
        "Gvgraph"_s,
        disklevel,
        ID_GV_OPCLASS_TYPE_GENERAL,
        ID_GV_OPGROUP_TYPE_GENERAL,
        0,
        auto_bitmap("res/icons/Gvgraph.png")
    );
}
