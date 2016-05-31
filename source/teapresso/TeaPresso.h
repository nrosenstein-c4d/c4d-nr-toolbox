/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#ifndef TEAPRESSO_H
#define TEAPRESSO_H

    #include "misc/legacy.h"
    //#include <c4d.h>

    // Include the resource descriptions of TeaPresso nodes.
    #include "res/description/Tvbase.h"
    #include "res/description/Tvbasecondition.h"
    #include "res/description/Tvroot.h"
    #include "res/description/Tvcurrentdocument.h"
    #include "res/description/Tvcontainer.h"
    #include "res/description/Tveachdocument.h"
    #include "res/description/Tveachobject.h"
    #include "res/description/Tveachtag.h"
    #include "res/description/Tvcondition.h"
    #include "res/description/Tvif.h"
    #include "res/description/Tvthen.h"
    #include "res/description/Tvelse.h"
    #include "res/description/Tvprintcontext.h"
    #include "res/description/Tvchecktype.h"
    #include "res/description/Tvisselected.h"
    #include "res/description/Tvselect.h"

    // =======================================================================
    // ===== Constants =======================================================
    // =======================================================================

    #define ACTIVEOBJECTMODE_TEAPRESSO  ACTIVEOBJECTMODE(1030257)
    #define TVPLUGIN_EXPRESSION         (1 << 0)
    #define TVPLUGIN_CONDITION          (1 << 1)

    #define TEAPRESSO_HPADDING          2
    #define TEAPRESSO_VPADDING          2
    #define TEAPRESSO_ICONSIZE          16
    #define TEAPRESSO_COLUMN_MAIN       1000
    #define TEAPRESSO_COLUMN_ENABLED    2000
    #define TEAPRESSO_COLUMN_CUSTOM     3000

    #define TEAPRESSO_CONTEXT_START     40000000

    /**
     * These are the ids of the three standart tabs: General, Conditions
     * and Expressions.
     */
    #define TEAPRESSO_FOLDER_GENERAL        1030294
    #define TEAPRESSO_FOLDER_CONDITIONS     1030295
    #define TEAPRESSO_FOLDER_EXPRESSIONS    1030296
    #define TEAPRESSO_FOLDER_ITERATORS      1030305
    #define TEAPRESSO_FOLDER_CONTEXTS       1030309
    // #define TEAPRESSO_FOLDER_PATTERNS       1030310

    /**
     * Use with SpecialEventAdd() to notify the tree-view of TeaPresso
     * it requires to be refreshed.
     */
    #define MSG_TEAPRESSO_UPDATETREEVIEW 1030293

    /**
     * Send to notify the node it must do set-up work. The condition node
     * for example creates it's if, then and else nodes on this message.
     */
    #define MSG_TEAPRESSO_SETUP 1030274

    // =======================================================================
    // ===== Helper Classes ==================================================
    // =======================================================================

    /**
     * Represents a rectangle in 2D space in absolute coordinates.
     */
    struct TvRectangle {
        LONG x1, y1, x2, y2;
    };


    // =======================================================================
    // ===== Cinema Plugin Integration =======================================
    // =======================================================================

    /**
     * Public interface for TeaPresso plugin objects.
     */
    class TvNode : public BaseList2D {

    private:

        TvNode();
        ~TvNode();

    public:

        BaseList2D* Execute(TvNode* root, BaseList2D* context);

        Bool AskCondition(TvNode* root, BaseList2D* context);

        Bool PredictContextType(LONG type);

        Bool AcceptParent(TvNode* other);

        Bool AcceptChild(TvNode* other);

        Bool AllowRemoveChild(TvNode* child);

        void DrawCell(LONG column, const TvRectangle& rect,
                      const TvRectangle& real_rect, DrawInfo* drawinfo,
                      const GeData& bgColor);

        LONG GetColumnWidth(LONG column, GeUserArea* area);

        LONG GetLineHeight(LONG column, GeUserArea* area);

        void CreateContextMenu(LONG column, BaseContainer* bc);

        Bool ContextMenuCall(LONG column, LONG command, Bool* refreshTree);

        String GetDisplayName() const;

        /**
         * Sends MSG_TEAPRESSO_SETUP to the node. Must be called after
         * inserting a *newly* created node (not a node that existed
         * before already).
         */
        Bool SetUp();

        /**
         * @return TRUE if the node is enabled, FALSE if not.
         */
        Bool IsEnabled(Bool checkParents=TRUE);

        /**
         * @param enabled TRUE if the node should be enabled, FALSE if
         *        it should be disabled.
         */
        void SetEnabled(Bool enabled);

        /**
         * Checks if the node is an inverted condition. The node's
         * implementation is responsible for implementing this case
         * (ie. inverting it's return-value). Only useful for conditions.
         */
        Bool IsInverted() const;

        /**
         * Sets if the node is an inverted condition. Only usefule
         * for conditions (not expressions).
         */
        void SetInverted(Bool inverted);

        /**
         * @param color Memory-location to be filled with the node's
         *        display-color.
         * @param depends TRUE when the display-mode should be taken
         *        in account.
         * @return TRUE when the color was filled, FALSE if not. The
         * color will not be filled when the color-mode is set to auto.
         */
        Bool GetDisplayColor(Vector* color, Bool depends=TRUE);

        /**
         * Sets the display-color of the node. Does not affect the
         * display-color mode!
         *
         * @param color The new display-color.
         */
        void SetDisplayColor(Vector color);

        /**
         * Validates the context-type safety of the tree with the new
         * parent node. This method will **not** check if the passed
         * *newParent* is a child of the calling node somewhere in
         * the hierarchy. If this is the fact, the results will be
         * unpredictable. Use TvFindChild() to check if a node
         * is a child-node of another hierarchy.
         *
         * @param newParent The new parent node to check the
         *        tree-structure with. Passing NULL will validate
         *        the existing nodes only.
         * @param checkRemoval Additionally check if the node
         *        is actually allowed to be removed, moving it to
         *        another parent doesn't make much sense in most cases,
         *        otherwise.
         * @return The node that did not accept something.
         */
        TvNode* ValidateContextSafety(TvNode* newParent, Bool checkRemoval=TRUE);

        /* BaseList2D Overrides */

        TvNode* GetDown() {
            return (TvNode*) BaseList2D::GetDown();
        }

        TvNode* GetNext() {
            return (TvNode*) BaseList2D::GetNext();
        }

        TvNode* GetPred() {
            return (TvNode*) BaseList2D::GetPred();
        }

        TvNode* GetUp() {
            return (TvNode*) BaseList2D::GetUp();
        }

        static TvNode* Alloc(LONG typeId);

        static void Free(TvNode*& ptr);

    };

    /**
     * Plugin interface class for TeaPresso nodes.
     */
    class TvOperatorData : public NodeData {

        typedef NodeData super;

    public:

        TvOperatorData() : NodeData() {}

        virtual ~TvOperatorData() {}

        /**
         * Called within the execution pipeline. The default implementation
         * does nothing at all. Required for expression nodes.
         *
         * @param host The node's host object. Cinema 4D owns the
         *        pointed object.
         * @param doc The context the node should be applied to.
         * @return The context to continue operating with.
         */
        virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context);

        /**
         * Called within the execution pipeline from an arbitrary node
         * to check if the condition this node represents evaluates to TRUE
         * or FALSE. Required for condition nodes.
         *
         * @param host The node's host object. Cinema 4D owns the
         *        pointed object.
         * @param context The context the node should be applied to.
         * @return The state of the condition.
         */
        virtual Bool AskCondition(TvNode* host, TvNode* root, BaseList2D* context);

        /**
         * @return TRUE when the passed type identifer is a possible
         * context-type forwarded to child-nodes.
         */
        virtual Bool PredictContextType(TvNode* host, LONG type);

        /**
         * @return TRUE if *other* would be accepted to be parent
         * of the host object.
         */
        virtual Bool AcceptParent(TvNode* host, TvNode* other);

        /**
         * Called when attempting to drag a TeaPresso node under the
         * *host* node. This method is called for each object in
         * a drag-array. If only one of the objects is not accepted,
         * the whole drag-operation is abandoned.
         *
         * The default implementation returns always TRUE.
         *
         * Note: It is important that this function returns TRUE
         * for each object that *is already* a child-object if the node.
         *
         * @param host The node's host object. Cinema 4D owns the
         *        pointed object.
         * @param child The child to be inserted.
         * @return TRUE if the child is accepted, FALSE if not.
         */
        virtual Bool AcceptChild(TvNode* host, TvNode* other);

        /**
         * @return TRUE when the child object may be removed, FALSE
         * if not. The default implementation returns TRUE always.
         */
        virtual Bool AllowRemoveChild(TvNode* host, TvNode* child);

        /**
         * Called to render the node in the tree-view cell. The
         * default implementation renders the default visuals. Note
         * that the drawing coordinates are calculated with padding.
         * Use the passed DrawInfo pointer to obtain the real
         * dimensions of the cell.
         *
         * @param host The node's host object. Cinema 4D owns the
         *        pointed object.
         * @param column The column in the tree-view.
         * @param rect The rectangle area of the cell with padding.
         * @param real_rect The rectangle area of the cell without padding.
         * @param drawinfo Structure containing information about the
         *        current drawing process.
         * @param bgColor The standart background-color for the current
         *        line and column.
         */
        virtual void DrawCell(
                    TvNode* host, LONG column, const TvRectangle& rect,
                    const TvRectangle& real_rect, DrawInfo* drawinfo,
                    const GeData& bgColor);

        /**
         * @return The width of the cell in the passed column index.
         */
        virtual LONG GetColumnWidth(
                    TvNode* host, LONG column, GeUserArea* area);

        /**
         * @return The height of the cell in the passed column index.
         */
        virtual LONG GetLineHeight(
                    TvNode* host, LONG column, GeUserArea* area);

        /**
         * Called to add entries to the passed BaseContainer used
         * for displaying the context menu. The inserted IDs
         * must be equal or greater than TEAPRESSO_CONTEXT_START to
         * be recognized.
         *
         * @param host The node's host object. Cinema 4D owns the
         *        pointed object.
         * @param column The column id the right-click was invoked at.
         * @param bc The container to add items to.
         */
        virtual void CreateContextMenu(
                    TvNode* host, LONG column, BaseContainer* bc);

        /**
         * Called when an item was clicked in the context-menu of
         * the node.
         *
         * @param host The node's host object. Cinema 4D owns the
         *        pointed object.
         * @param column The column id the right-click was invoked at.
         * @param command The command id of the clicked entry.
         * @param refreshTree Set this to TRUE when the tree-view
         *        should be refreshed.
         * @return TRUE when the call was handled, FALSE if not.
         */
        virtual Bool ContextMenuCall(
                    TvNode* host, LONG column, LONG command,
                    Bool* refreshTree);

        /**
         * Return a display-name for the node.
         */
        virtual String GetDisplayName(const TvNode* host) const;

        /**
         * Convenient method called on MSG_DESCRIPTION_COMMAND.
         */
        virtual Bool OnDescriptionCommand(
                    TvNode* host, const DescriptionCommand& data);

        /* NodeData Overrides */

        virtual Bool Init(GeListNode* node);

        virtual Bool IsInstanceOf(const GeListNode* node, LONG type) const;

        virtual Bool Message(GeListNode* node, LONG type, void* pData);

        virtual Bool GetDEnabling(
                GeListNode* node, const DescID& id, const GeData& tData,
                DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);

    };

    // =======================================================================
    // ===== Plugin Registration =============================================
    // =======================================================================

    /**
     * TeaPresso plugin function table.
     */
    struct TVPLUGIN : public NODEPLUGIN {
        BaseList2D* (TvOperatorData::*Execute)(
                TvNode* host, TvNode* root, BaseList2D* context);
        Bool (TvOperatorData::*AskCondition)(
                TvNode* host, TvNode* root, BaseList2D* context);
        Bool (TvOperatorData::*PredictContextType)(
                TvNode* host, LONG type);
        Bool (TvOperatorData::*AcceptParent)(
                TvNode* host, TvNode* other);
        Bool (TvOperatorData::*AcceptChild)(
                TvNode* host, TvNode* other);
        Bool (TvOperatorData::*AllowRemoveChild)(
                TvNode* host, TvNode* child);
        void (TvOperatorData::*DrawCell)(
                TvNode* host, LONG column, const TvRectangle& rect,
                const TvRectangle& real_rect, DrawInfo* drawinfo,
                const GeData& bgColor);
        LONG (TvOperatorData::*GetColumnWidth)(
                TvNode* host, LONG column, GeUserArea* area);
        LONG (TvOperatorData::*GetLineHeight)(
                TvNode* host, LONG column, GeUserArea* area);
        void (TvOperatorData::*CreateContextMenu)(
                TvNode* host, LONG column, BaseContainer* bc);
        Bool (TvOperatorData::*ContextMenuCall)(
                TvNode* host, LONG column, LONG command,
                Bool* refreshTree);
        String (TvOperatorData::*GetDisplayName)(
                const TvNode* host) const;
    };

    /**
     * Register a TvOperatorData plugin.
     *
     * @param id The unique plugin id.
     * @param name The name of the plugin.
     * @param info Flags for the plugin. The default pluginflags and the
     *        the followings are supported:
     *
     *        - TVPLUGIN_CONDITION
     *        - TVPLUGIN_EXPRESSION
     * @param alloc A callback allocating an instance of your plugin data.
     * @param desc The name of the plugin description.
     * @param icon An icon for the plugin.
     * @param disklevel The plugin's disklevel.
     * @param destFolder 0 To let automatically determine the folder, -1
     *        to not add the plugin to a folder. Or a folder id to add the
     *        plugin to.
     * @return TRUE if the plugin was successfully registered, FALSE
     * if not.
     */
    Bool TvRegisterOperatorPlugin(
                LONG id, const String& name, LONG info, DataAllocator* alloc,
                const String& desc, BaseBitmap* icon, LONG disklevel,
                LONG destFolder=0);

    // =======================================================================
    // ===== Library Function Definitions ====================================
    // =======================================================================

    /**
     * @return TRUE when TeaPresso is installed, FALSE if not.
     */
    Bool TvInstalled();

    /**
     * Returns the active TeaPresso hierarchy.
     *
     * @return The root of the active TeaPresso hierarchy. May be NULL.
     */
    TvNode* TvGetActiveRoot();

    /**
     * Returns the BaseContainer that displays the hierarchy of
     * installed TeaPresso plugins. TvRegisterOperatorPlugin() will
     * automatically register the node in the "Generals" tab (with
     * id TEAPRESSO_FOLDER_GENERAL), unless its *doAddTab* parameter is
     * set to FALSE.
     *
     * Every item in the container must have its own unique identifer
     * obtained from the plugincafe.
     *
     * @return The internal container address.
     */
    BaseContainer* TvGetFolderContainer();

    /**
     * Creates a hierarchy consisting of BaseList2D objects, representing
     * the folders, and TvNode objects representing the plugins. The
     * plugins are not set up at this point (MSG_TEAPRESSO_SETUP has not
     * been sent).
     *
     * @param bc The container to construct the hierarchy from. Errors
     *        are ignored. Passing NULL will use the global container.
     * @return The hierarchy.
     */
    TvNode* TvCreatePluginsHierarchy(const BaseContainer* bc=NULL);

    /**
     * Update the TvManager tree-view(s).
     */
    inline void TvUpdateTreeViews() {
        SpecialEventAdd(MSG_TEAPRESSO_UPDATETREEVIEW);
    }

    /**
     * Brings the Atrribute Manager to show TeaPresso nodes.
     *
     * @param arr An optional array of TvNode's that will be displayed
     *        in the attribute manager.
     */
    void TvActivateAM(const AtomArray* arr=NULL);

    /**
     * Brings the Attribute Manager to show the passed TeaPresso node.
     *
     * @param node The TeaPresso node to show.
     */
    inline void TvActivateAM(TvNode* node) {
        AutoAlloc<AtomArray> arr;
        arr->Append(node);
        TvActivateAM(arr);
    }


#endif /* TEAPRESSO_H */
