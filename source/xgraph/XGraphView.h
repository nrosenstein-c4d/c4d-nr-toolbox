/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#ifndef NR_XGRAPH_VIEW_H
#define NR_XGRAPH_VIEW_H

    #include <c4d.h>

    /**
     * State flags for values returned by XGraphData subclasses.
     */
    typedef Char XGraphValueState;

    /**
     * Empty bitfield.
     */
    static const XGraphValueState XGraphValueState_Empty = 0;

    /**
     * Bit flag for marking the returned value as valid.
     */
    static const XGraphValueState XGraphValueState_Valid = (1 << 0);

    /**
     * Bit flag for marking the returned value as outdated. It is rendered
     * in a different color.
     */
    static const XGraphValueState XGraphValueState_Outdated = (1 << 1);


    /**
     * Simple storage class representing a rectangular area on the screen.
     */
    class XGraphRect {

    public:

        Int32 x1, y1, x2, y2;

        XGraphRect() : x1(0), y1(0), x2(0), y2(0) {}

        XGraphRect(Int32 x1, Int32 y1, Int32 x2, Int32 y2) : x1(x1), y1(y1), x2(x2), y2(y2) {}

        virtual ~XGraphRect() {}

        Bool Valid() const {
            return x1 <= x2 && y1 <= y2;
        }

        Bool HasSpace() const {
            return x1 < x2 && y1 < y2;
        }

        Int32 Width() const {
            return x2 - x1;
        }

        Int32 Height() const {
            return y2 - y1;
        }

    };

    /**
     * This class represents a data set that can be represented visually
     * in a graph view.
     */
    class XGraphData {

    public:

        /**
         * Return the range of the graph.
         */
        virtual void GetRange(Float* x_min, Float* x_max, Float* y_min, Float* y_max) const = 0;

        /**
         * Return the value of the graph at position *x*.
         *
         * @param x The x position to return the value of the graph for.
         *          The interpolation must be done by hand.
         * @param state Assign the state of the value. This can be a
         *          combination of the XGraphValueState flags.
         */
        virtual Float GetValue(Float x, XGraphValueState* state) const = 0;

    };

    /**
     * For future extension, may contain settings that are to be respected
     * by the render engine.
     */
    struct XGraphDisplay {

        /**
         * Only valid in the graph-drawing pass. Contains the color
         * for the graph to be drawn.
         */
        GeData graph_color;

        /**
         * Only valid in the graph-drawing pass. The X axis minimum value of
         * the graph view.
         */
        Float x_min;

        /**
         * Only valid in the graph-drawing pass. The X axis maxmimum value of
         * the graph view.
         */
        Float x_max;

        /**
         * Only valid in the graph-drawing pass. The Y axis minimum value of
         * the graph view.
         */
        Float y_min;

        /**
         * Only valid in the graph-drawing pass. The Y axis maxmimum value
         * of the graph view.
         */
        Float y_max;

        /**
         * Only valid in the graph-drawing pass. Contains the index
         * of the graph that is currently drawn.
         */
        Int32 graph_index;

        /**
         * Only valid in the graph-drawing pass. Contains the number
         * of graphs that are to be drawn.
         */
        Int32 graph_count;


        /**
         * Always valid. The color for an "outdated" graph value.
         */
        GeData graph_color_outdated;

        /**
         * Color of the grid in the background.
         */
        GeData grid_color;

        /**
         * Always valid. The color of the graph axises.
         */
        GeData axis_color;

        /**
         * Always valid. The color for text.
         */
        GeData text_color;

        /**
         * Padding pixels for text.
         */
        Int32 t_pad[2];

        /**
         * Set in XGraphView::DrawGraphEnd().
         */
        struct {
            Int32 top, right, bottom, left;
        } padding;

        /**
         * Set in XGraphView::DrawGraphEnd(). Computed by FillTextValues().
         */
        String t_x_min;
        String t_x_max;
        String t_y_min;
        String t_y_max;

        XGraphDisplay() {
            t_pad[0] = t_pad[1] = 2;
        }

        /**
         * Fills in the text values for the axis minimum and maximum values.
         */
        void FillTextValues() {
            t_x_min = String::FloatToString(x_min);
            t_x_max = String::FloatToString(x_max);
            t_y_min = String::FloatToString(y_min);
            t_y_max = String::FloatToString(y_max);
        }

    };

    /**
     * Abstract base class implementing the rendering algorithms for a graph.
     */
    class XGraphRenderEngine {

    public:

        static void* operator new (size_t size) noexcept {
            iferr (void* mem = NewMemClear(char, size))
                return nullptr;
            return mem;
        }

        static void operator delete (void* p) {
            DeleteMem(p);
        }


        /**
         * Draw the background of the graph view.
         */
        virtual void DrawBackground(GeUserArea* area, const XGraphRect& rect,
                                    const XGraphDisplay& display);

        /**
         * Draw the background of the graph into the graph view.
         */
        virtual void DrawGraphBackground(GeUserArea* area, const XGraphRect& rect,
                                         const XGraphDisplay& display) {
        }

        /**
         * Draw the X and Y axises into the graph view.
         */
        virtual void DrawAxises(GeUserArea* area, const XGraphRect& rect,
                                const XGraphRect& g_rect, const XGraphDisplay& display);

        /**
         * Draw a graph into graph view.
        */
        virtual void DrawGraph(GeUserArea* area, const XGraphRect& rect,
                               const XGraphDisplay& display, const XGraphData* graph);

    };

    /**
     * This is the base class that manages drawing of a graph into a
     * GeUserArea. It can be subclassed to achieve custom effects.
     */
    class XGraphView {

        typedef struct _GraphData {
            const XGraphData* graph;
            GeData color;
            XGraphDisplay* display_override;
        } _GraphData;

        Bool draw_begun;
        XGraphRenderEngine* engine;
        maxon::BaseArray<_GraphData> graphs;

        XGraphDisplay display;
        XGraphRect rect;
        XGraphRect g_rect;
        GeUserArea* area;

    public:

        XGraphView(XGraphRenderEngine* engine=NULL) : draw_begun(FALSE) {
            if (!engine) engine = new XGraphRenderEngine;
            this->engine = engine;
        };

        virtual ~XGraphView() {
            if (engine) {
                delete engine;
                engine = NULL;
            }
        }

        /**
         * Invokes the drawing passes sequentially right before the point that
         * graphs can be drawn into the graph view. DrawGraph() can be called
         * when this mtehod returned TRUE.
         *
         * @param area The GeUserArea to draw into.
         * @param rect The rectangular area of the complete graph viewport.
         * @param display Display information. Only the values that are always
         *      valid (see documentation of XGraphDisplay) are used.
         * @return TRUE if the the drawing action has begun, FALSE if an error
         * occured.
         */
        Bool DrawGraphBegin(GeUserArea* area, const XGraphRect& rect,
                            const XGraphDisplay& display);

        /**
         * Draw a graph into the graph view. The action is internally and
         * completed at a call to DrawGraphEnd(). The passed *data* parameter
         * must therefore not be deallocated until DrawGraphEnd() has been
         * called.
         *
         * DrawGraphBegin() must have been called before.
         *
         * @param graph The graph to draw.
         * @param color The color for the graph.
         * @param display_override Possible override for the display
         * information.
         */
        void DrawGraph(const XGraphData* graph, const GeData& color,
                       XGraphDisplay* display_override=NULL);

        /**
         * End the drawing sequence of graphs. This must be called when an
         * opening call to DrawGraphBegin() has been made.
         */
        void DrawGraphEnd();

    };

    /**
     * Draws text aligned to a point depending on the frame bounds.
     */
    void XGraphDrawBoundText(GeUserArea* area, String text, Int32 pos[2],
                 const Int32 padding[2], const XGraphRect& bounds);

#endif /* NR_XGRAPH_VIEW_H */