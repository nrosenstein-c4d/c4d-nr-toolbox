/* Copyright (c) 2016 Niklas Rosenstein
 * All rights reserved. */

#include <c4d.h>
#include <NiklasRosenstein/macros.hpp>
#include <NiklasRosenstein/c4d/utils.hpp>
#include <NiklasRosenstein/c4d/fmap.hpp>
#include <NiklasRosenstein/c4d/iterators.hpp>
#include <customgui_nrcolorpalette.h>
#include <xcolor.h>
#include "timehide.h"
#include "res/description/Hnrtoolbox.h"

namespace nr { using namespace niklasrosenstein; }

using nr::c4d::get_param;
using nr::c4d::set_param;

static Bool DrawObjectUVs(PolygonObject* op, BaseDraw* bd)
{
  if (!op || op->GetType() != Opolygon)
    return false;

  UVWTag* tag = static_cast<UVWTag*>(op->GetTag(Tuvw));
  CPolygon const* polys = op->GetPolygonR();
  Vector const* points = op->GetPointR();
  if (!polys || !points || !tag)
    return false;


  bd->SetMatrix_Matrix(op, op->GetMg());
  Int32 const count = op->GetPolygonCount();
  ConstUVWHandle uvwhandle = tag->GetDataAddressR();
  for (Int32 i = 0; i < count; ++i) {
    CPolygon const& f = polys[i];
    Vector p[4] = {points[f.a], points[f.b], points[f.c], points[f.d]};
    UVWStruct c;
    UVWTag::Get(uvwhandle, i, c);
    c.a.z = c.b.z = c.c.z = c.d.z = 0.0;
    bd->DrawPolygon(p, &c.a, f.c != f.d);
  }
	return true;
}

class ToolboxHook : public SceneHookData
{
  typedef SceneHookData super;
	THStats* thstats = nullptr;

public:

	static GeData ActiveObjectManager_ToolBoxHook(BaseContainer const& msg, void* pdata)
	{
		switch (msg.GetId()) {
			case AOM_MSG_ISENABLED:
				return true;
			case AOM_MSG_GETATOMLIST: {
				BaseDocument* doc = GetActiveDocument();
				auto* data = reinterpret_cast<AtomArray*>(pdata);
				if (!doc || !data) break;
				auto* hook = doc->FindSceneHook(Hnrtoolbox);
				if (hook) data->Append(hook);
				return true;
			}
		}
		return false;
	}


  static NodeData* Alloc() { return NewObjClear(ToolboxHook); }

	static Bool Register()
	{
		if (!ActiveObjectManager_RegisterMode((ACTIVEOBJECTMODE) Hnrtoolbox, "nr:Workflow", ActiveObjectManager_ToolBoxHook))
    	return false;
		if (!RegisterDescription(Hnrtoolbox, "Hnrtoolbox"_s))
			return false;
		Int32 const info = PLUGINFLAG_SCENEHOOK_NOTDRAGGABLE;
		Int32 const priority = EXECUTIONPRIORITY_INITIAL;
		Int32 const disklevel = 0;
		return RegisterSceneHookPlugin(
			Hnrtoolbox,
			"nr:Workflow"_s,
			info,
			ToolboxHook::Alloc,
			priority,
			disklevel);
	}

public: // SceneHookData Overrides

  virtual Bool Draw(BaseSceneHook* hook, BaseDocument* doc, BaseDraw* bd,
    BaseDrawHelp* bh, BaseThread* bt, SCENEHOOKDRAW flags) override
  {
    BaseContainer* data = hook->GetDataInstance();
    if (!data) return false;

    NR_IF (data->GetBool(NRTOOLBOX_HOOK_VIEWPORT_SHOWUVS)) {
			if (bd->GetDrawPass() != DRAWPASS_OBJECT) break;
			bd->SetLightList(BDRAW_SETLIGHTLIST_NOLIGHTS);
			for (auto& op : nr::c4d::iter_hierarchy<BaseObject>(doc->GetFirstObject(), false)) {
				for_each_polyobj(op, true, false, [bd](PolygonObject* obj) {
					DrawObjectUVs(obj, bd);
					return true;
				});
			}
    }

    return true;
  }

	virtual EXECUTIONRESULT Execute(BaseSceneHook* hook, BaseDocument* doc,
		BaseThread* bt, Int32 priority, EXECUTIONFLAGS) override
	{
		NR_IF (get_param(hook, NRTOOLBOX_HOOK_SWATCHES_SYNCHRONIZE).GetBool()) {
			// TODO: Filter calls in which the synchronization is performed
			// TODO: Support shader updates on nodes other than materials
			GeData data = get_param(hook, NRTOOLBOX_HOOK_SWATCHES);
			nr::ColorPaletteData* colors = nr::ColorPaletteData::Get(data);
			if (!colors) break;
			for (auto* mat = doc->GetFirstMaterial(); mat; mat = mat->GetNext()) {
				for (auto& shader : nr::c4d::iter_hierarchy<BaseShader>(mat->GetFirstShader(), false)) {
					if (shader->GetType() != Xcolor) continue;
					String name = shader->GetName();
					if (name.SubStr(0, 7).ToLower() != "swatch:") continue;
					name = name.SubStr(7, name.GetLength() - 7);
					nr::Swatch const* swatch = colors->Find(name);
					if (swatch) {
						set_param(shader, COLORSHADER_COLOR, swatch->color);
					}
				}
			}
		}
		return EXECUTIONRESULT_OK;
	}

public: // NodeData Overrides

  virtual Bool Init(GeListNode* node) override
	{
    if (!node) return false;
    BaseContainer* data = ((BaseSceneHook*) node)->GetDataInstance();
    if (!data) return false;
    data->SetBool(NRTOOLBOX_HOOK_VIEWPORT_SHOWUVS, false);
    data->SetBool(NRTOOLBOX_HOOK_SAFEFRAME_ENABLED, true);
    data->SetVector(NRTOOLBOX_HOOK_SAFEFRAME_COLOR, Vector());
    data->SetFloat(NRTOOLBOX_HOOK_SAFEFRAME_ALPHA, 1.0);
    data->SetBool(NRTOOLBOX_HOOK_SAFEFRAME_BORDER, false);
    data->SetVector(NRTOOLBOX_HOOK_SAFEFRAME_BORDERCOLOR, Vector(0.4));
		data->SetBool(NRTOOLBOX_HOOK_TIMEHIDE_ONLYSELECTED, false);
		data->SetBool(NRTOOLBOX_HOOK_TIMEHIDE_ONLYANIMATED, false);
		data->SetInt32(NRTOOLBOX_HOOK_TIMEHIDE_TRACKS, NRTOOLBOX_HOOK_TIMEHIDE_TRACKS_SHOWALL);
		data->SetBool(NRTOOLBOX_HOOK_SWATCHES_SYNCHRONIZE, true);
		data->SetData(NRTOOLBOX_HOOK_SWATCHES, GeData(CUSTOMGUI_NR_COLORPALETTE, DEFAULTVALUE));
    return true;
  }

	virtual void Free(GeListNode* node) override
	{
		THFreeStats(this->thstats);
		super::Free(node);
	}

  virtual Bool GetDEnabling(GeListNode* node, const DescID& id,
    const GeData& t_data, DESCFLAGS_ENABLE flags,
    const BaseContainer* itemdesc) override
  {
    switch (id[0].id) {
      case NRTOOLBOX_HOOK_SAFEFRAME_BORDERCOLOR:
        return get_param(node, NRTOOLBOX_HOOK_SAFEFRAME_BORDER).GetBool();
    }
    return super::GetDEnabling(node, id, t_data, flags, itemdesc);
  }

	virtual Bool Message(GeListNode* node, Int32 msg, void* pdata) override
	{
		this->thstats = THMessage(this->thstats, static_cast<BaseSceneHook*>(node), node->GetDocument(), msg, pdata);
		return super::Message(node, msg, pdata);
	}

};

/* Message plugin that redirects #EVMSG_CHANGE to the #ToolboxHook. */
class MessageHandler : public MessageData
{
public: // MessageData Overrides

 	enum
	{
		PLUGIN_ID = MSG_TIMEHIDE_EVMSG_CHANGE
	};

	static Bool Register()
	{
		return RegisterMessagePlugin(
			PLUGIN_ID,
			"nr-toolbox-MessageHandler"_s,
			0,
			NewObjClear(MessageHandler));
	}

  virtual Bool CoreMessage(Int32 type, const BaseContainer& msg)
	{
    switch (type) {
      case EVMSG_CHANGE: {
        // TODO: Maybe we can find a way to filter some of
        // the events? (eg. only when an Object was modified or
        // added, but currently it is also sent by a simple click
        // in the Object manager). See issue #11
        BaseDocument* doc = GetActiveDocument();
        BaseSceneHook* hook = doc ? doc->FindSceneHook(Hnrtoolbox) : nullptr;
        if (hook)
          hook->Message(MSG_TIMEHIDE_EVMSG_CHANGE);
        break;
      }
    }
    return true;
  }

};

/* VideoPost for the SafeFrame feature which draws the overlay. */
class SafeFrameVideoPost : public VideoPostData
{

	struct Rect
  {
		Int32 x1, y1, x2, y2;
		inline void shrink(Int32 n)
    {
			x1 += n; y1 += n;
			x2 -= n; y2 -= n;
		}
		inline void expand(Int32 n)
    {
			x1 -= n; y1 -= n;
			x2 += n; y2 += n;
		}
	};

  static Float32 OverlayColorValue(Float32 c1, Float32 c2, Float32 alpha)
  {
    return (c1 * (1.0f - alpha)) + (c2 * alpha);
  }

	// This rectangle is set when the VideoPost is being initialized
	// and is required for sampling on the non-safe frame of the rendering.
	Rect m_rect;

	// The color and border color as specified in the properties
	// of the VideoPost plugin.
	Vector m_color, m_border_color;

	// True if the the border should be drawn.
	Bool m_border;

	// The alpha value as specified in the properties of the VideoPost.
  Float m_alpha;

	// True if an Editor render is being performed, false if not. The
	// VideoPost should only take effect in the Editor rendering.
	Bool m_editor = false;

public:

	enum
	{
		PLUGIN_ID = 1037816,
	};

	static NodeData* Alloc() { return NewObjClear(SafeFrameVideoPost); }

	static Bool Register()
	{
		Int32 const priority = 0;
		return RegisterVideoPostPlugin(
      PLUGIN_ID,
			"nr-toolbox-SafeFrame"_s,
			PLUGINFLAG_VIDEOPOST_INHERENT,
      Alloc,
			""_s,
			priority,
			VPPRIORITY_EXTERNAL);
	}

public: // VideoPostData Overrides

	virtual VIDEOPOSTINFO GetRenderInfo(BaseVideoPost* node)
	{
		if (!m_editor) return VIDEOPOSTINFO_0;
		else return VIDEOPOSTINFO_EXECUTELINE;
	}

	virtual RENDERRESULT Execute(BaseVideoPost* node, VideoPostStruct* vps)
	{
		RENDERRESULT const ok = RENDERRESULT_OK;
		RENDERRESULT const failure = RENDERRESULT_OUTOFMEMORY;
		if (!vps->doc)
			return failure;

		// Only execute the post-processed rendering when the
		// rendering takes places in the Viewport.
		m_editor = !(vps->renderflags & RENDERFLAGS_EXTERNAL);
		if (!m_editor)
			return ok;

		// Only render on the closing VIDEOPOSTCALL_INNER.
		if (vps->vp != VIDEOPOSTCALL_INNER || !vps->open)
			return ok;

		// The SceneHook contains all the parameters of the VideoPost.
		BaseSceneHook* hook = vps->doc->FindSceneHook(Hnrtoolbox);
		if (!hook)
			return ok;

		// Don't render if its not enabled.
		if (!get_param(hook, NRTOOLBOX_HOOK_SAFEFRAME_ENABLED).GetBool())
			return ok;

		// Retrieve the dimensions of the active view's safe-frame.
		BaseView* view = vps->doc->GetActiveBaseDraw();
		if (!view)
		  return ok;

		view->GetSafeFrame(&m_rect.x1, &m_rect.y1, &m_rect.x2, &m_rect.y2);
		m_rect.expand(1);

		// Get the color the non-safe frame of the rendering should
		// be shadowed with.
		m_color = get_param(hook, NRTOOLBOX_HOOK_SAFEFRAME_COLOR).GetVector();

		// And the specified alpha value.
		m_alpha = get_param(hook, NRTOOLBOX_HOOK_SAFEFRAME_ALPHA).GetFloat();

		// Retrieve the border information.
		m_border = get_param(hook, NRTOOLBOX_HOOK_SAFEFRAME_BORDER).GetBool();
		m_border_color = get_param(hook, NRTOOLBOX_HOOK_SAFEFRAME_BORDERCOLOR).GetVector();
		return ok;
	}

	virtual void ExecuteLine(BaseVideoPost* node, PixelPost* pp)
	{
		Int32 const y = pp->line;
		Int32 i, x;

		// This pointer represents the current color pixel. It is
		// incremented during iteration.
		Float32* col = pp->col;

		// If anti-aliasing is enabled, there are 4 subpixels for each
		// pixel, or only one real pixel if anti-aliasing is not enabled.
		Int32 const subpixels = (pp->aa ? 4 : 1);

		for (x = pp->xmin; x <= pp->xmax; x++) {
			// Determine if the current pixel is within the safe
			// frame or not.
			Bool safe    = x > m_rect.x1 && x < m_rect.x2;
			safe = safe && y > m_rect.y1 && y < m_rect.y2;

			if (safe) {
				// Update the pixel-buffer pointer as if it was
				// run through a loop that modifies each subpixel
				// (see below).
				col += pp->comp * subpixels;
				continue;
			}

			Bool on_border = (y == m_rect.y1 || y == m_rect.y2 ||
						x == m_rect.x1 || x == m_rect.x2);

			if (m_border && on_border) {
				// We'll draw the border on the left side of the AA pixel
				// only to get a decent and sharp border.
				for (i = 0; i < subpixels; i += 2) {
					(col + i * pp->comp)[0] = m_border_color.x;
					(col + i * pp->comp)[1] = m_border_color.y;
					(col + i * pp->comp)[2] = m_border_color.z;
				}
				col += pp->comp * subpixels;
			}
			else {
				// Otherwise, we'll overwrite all pixels to the
				// specified overlay color.
				for (i = 0; i < subpixels; i++, col += pp->comp) {
					col[0] = OverlayColorValue(col[0], m_color.x, m_alpha);
					col[1] = OverlayColorValue(col[1], m_color.y, m_alpha);
					col[2] = OverlayColorValue(col[2], m_color.z, m_alpha);
				}
			}
		}
	}

};

Bool RegisterNRWorkflow()
{
	if (!MessageHandler::Register()) return false;
	if (!SafeFrameVideoPost::Register()) return false;
	if (!ToolboxHook::Register()) return false;
	return true;
}
