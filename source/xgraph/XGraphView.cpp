/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include "XGraphView.h"
#include <c4d_apibridge.h>

void XGraphRenderEngine::DrawBackground(GeUserArea* area, const XGraphRect& rect,
            const XGraphDisplay& display) {
    if (display.grid_color.GetType() != DA_NIL) {
        area->DrawSetPen(display.grid_color);

        // TODO: Draw grid for the graph.
    }
}

void XGraphRenderEngine::DrawAxises(GeUserArea* area, const XGraphRect& rect,
            const XGraphRect& g_rect, const XGraphDisplay& display) {
    area->DrawSetPen(display.axis_color);

    Int32 x1, y1, x2, y2;
    Float xmid, ymid;

    Float x_range = display.x_max - display.x_min;
    Float y_range = display.y_max - display.y_min;
    Int32 width = g_rect.x2 - g_rect.x1;
    Int32 height = g_rect.y2 - g_rect.y1;

    Int32 x_axis, y_axis;
    x_axis = y_axis = -1;

    // X-axis
    if (y_range > 0.000001 && height > 0 && (display.y_min <= 0 && display.y_max >= 0)) {
        ymid = 1.0 + display.y_min / y_range;
        x1 = g_rect.x1;
        x2 = g_rect.x2;

        if (0 >= ymid || ymid <= 1) {
            y1 = y2 = y_axis = g_rect.y1 + static_cast<Int32>(ymid * height);
            area->DrawLine(x1, y1, x2, y2);
        }
    }

    // Y-axis
    if (x_range > 0.000001 && width > 0 && (display.x_min <= 0 && display.x_max >= 0)) {
        xmid = - display.x_min / x_range;
        y1 = g_rect.y1;
        y2 = g_rect.y2;

        if (0 >= xmid || xmid <= 1) {
            x1 = x2 = x_axis = g_rect.x1 + static_cast<Int32>(xmid * width);
            area->DrawLine(x1, y1, x2, y2);
        }
    }

    area->DrawSetPen(display.text_color);
    Int32 pos[2] = { x_axis, y_axis };

    pos[0] = x_axis;
    pos[1] = rect.y1;
    XGraphDrawBoundText(area, display.t_y_max, pos, display.t_pad, rect);
    pos[1] = rect.y2;
    XGraphDrawBoundText(area, display.t_y_min, pos, display.t_pad, rect);

    pos[0] = rect.x2;
    pos[1] = y_axis;
    XGraphDrawBoundText(area, display.t_x_max, pos, display.t_pad, rect);
    pos[0] = rect.x1;
    XGraphDrawBoundText(area, display.t_x_min, pos, display.t_pad, rect);
}

void XGraphRenderEngine::DrawGraph(GeUserArea* area, const XGraphRect& rect,
            const XGraphDisplay& display, const XGraphData* graph) {
    Float xrange = display.x_max - display.x_min;
    Float yrange = display.y_max - display.y_min;
    Int32 width = rect.Width();
    Int32 height = rect.Height();
    if (xrange <= 0 || width <= 0) return;
    if (yrange <= 0 || height <= 0) return;

    XGraphValueState prevstate = XGraphValueState_Empty;
    Float prev = graph->GetValue(display.x_min, &prevstate);

    for (Int32 i=1; i <= width; i++) {
        Float x = ((Float) i / (Float) width) * xrange + display.x_min;
        XGraphValueState state = XGraphValueState_Empty;
        Float y = graph->GetValue(x, &state);

        if (state & XGraphValueState_Valid && prevstate & XGraphValueState_Valid) {
            if (state & XGraphValueState_Outdated) {
                area->DrawSetPen(display.graph_color_outdated);
            }
            else {
                area->DrawSetPen(display.graph_color);
            }

            Int32 x1 = rect.x1 + i - 1;
            Int32 y1 = rect.y2 - Int32((prev - display.y_min) / yrange * height);
            Int32 x2 = x1 + 1;
            Int32 y2 = rect.y2 - Int32((y - display.y_min) / yrange * height);

            area->DrawLine(x1, y1, x2, y2);
        }
        prev = y;
        prevstate = state;
    }
}


Bool XGraphView::DrawGraphBegin(GeUserArea* area, const XGraphRect& rect,
            const XGraphDisplay& display) {
    if (!engine || !area) {
        return FALSE;
    }
    if (!rect.HasSpace()) {
        return FALSE;
    }

    this->rect = rect;
    this->display = display;
    this->area = area;

    graphs.Flush();
    engine->DrawBackground(area, rect, display);

    this->display.x_min = 0;
    this->display.x_max = 0;
    this->display.y_min = 0;
    this->display.y_max = 0;
    draw_begun = TRUE;
    return TRUE;
}

void XGraphView::DrawGraph(const XGraphData* graph, const GeData& color,
            XGraphDisplay* display_override) {
    if (!draw_begun || !engine || !graph) return;

    _GraphData data;
    data.graph = graph;
    data.color = color;
    data.display_override = display_override;
    graphs.Append(std::move(data));

    Float x_min, x_max, y_min, y_max;
    x_min = x_max = y_min = y_max = 0.0;

    graph->GetRange(&x_min, &x_max, &y_min, &y_max);
    if (x_min < display.x_min) display.x_min = x_min;
    if (y_min < display.y_min) display.y_min = y_min;
    if (x_max > display.x_max) display.x_max = x_max;
    if (y_max > display.y_max) display.y_max = y_max;
}

void XGraphView::DrawGraphEnd() {
    if (!draw_begun || !engine) return;

    Int32 count = graphs.GetCount();
    display.graph_count = count;

    if (display.x_max - display.x_min < 0.00000001) {
        display.x_min = 0;
        display.x_max = 1;
    }
    if (display.y_max - display.y_min < 0.00000001) {
        display.y_max = 1;
        display.y_min = 0;
    }

    display.FillTextValues();
    display.padding.left = area->DrawGetTextWidth(display.t_x_min) + display.t_pad[0];
    display.padding.top = area->DrawGetFontHeight() + display.t_pad[1];
    display.padding.right = area->DrawGetTextWidth(display.t_y_min) + display.t_pad[0];
    display.padding.bottom = area->DrawGetFontHeight() + display.t_pad[1];

    g_rect = rect;
    g_rect.x1 += display.padding.left;
    g_rect.y1 += display.padding.top;
    g_rect.x2 -= display.padding.right;
    g_rect.y2 -= display.padding.bottom;
    if (g_rect.HasSpace()) {
        engine->DrawGraphBackground(area, g_rect, display);
    }

    engine->DrawAxises(area, rect, g_rect, display);

    for (Int32 i=0; i < count; i++) {
        const _GraphData& data = graphs[i];
        XGraphDisplay* g_display = &display;
        if (data.display_override) g_display = data.display_override;

        g_display->graph_color = data.color;
        g_display->graph_index = i;

        engine->DrawGraph(area, g_rect, *g_display, data.graph);
    }

    graphs.Flush();
    area = NULL;
    draw_begun = FALSE;
}


void XGraphDrawBoundText(GeUserArea* area, String text, Int32 pos[2],
            const Int32 padding[2], const XGraphRect& bounds) {
    if (!area || c4d_apibridge::IsEmpty(text) || !bounds.HasSpace()) return;
    Int32 width = area->DrawGetTextWidth(text) + padding[0];
    Int32 height = area->DrawGetFontHeight() + padding[0];

    Bool orientation[2] = { TRUE };

    if (pos[0] + width > bounds.x2) {
        pos[0] -= width;
    }
    else {
        pos[0] += padding[0];
    }

    if (pos[1] + height > bounds.y2) {
        pos[1] -= width;
    }
    else {
        pos[1] += padding[1];
    }

    area->DrawText(text, pos[0], pos[1]);
}



