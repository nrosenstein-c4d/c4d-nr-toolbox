// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#include <c4d.h>
#include <NiklasRosenstein/macros.hpp>
#include <NiklasRosenstein/c4d/gui.hpp>
#include <NiklasRosenstein/c4d/cleanup.hpp>
#include <NiklasRosenstein/c4d/customgui.hpp>
#include <NiklasRosenstein/c4d/iterators.hpp>
#include "customgui_nrcolorpalette.h"
#include "misc/print.h"

using nr::SwatchBase;
using nr::Swatch;
using nr::SwatchFolder;
using nr::ColorPaletteData;

namespace nr { using namespace niklasrosenstein; }

static Vector g_last_color;
static Float g_last_brightness = 1.0;
static BaseContainer* g_last_layout = nullptr;
static nr::c4d::cleanup_handler __cleanup([] {
  DeleteObj(g_last_layout);
});

enum
{
  GADGET_MAINGROUP = 1000,
  GADGET_USERAREA,
  GADGET_SWATCH_NAME,
  GADGET_COLORCHOOSER,
  GADGET_SHOWCOLORCHOOSER,

  GUIMSG_SELECTION_CHANGED = 2000,
  GUIMSG_VALUE_CHANGED,
};

struct ColorPaletteDataTristate
{
  TriState<GeData> data;
  inline ColorPaletteDataTristate() { }
  inline ColorPaletteDataTristate(TriState<GeData> const& data) : data(data) { }
  NR_OPERATOR_COPY_ASSIGNMENT(ColorPaletteDataTristate);
  inline operator TriState<GeData> const& () const { return this->data; }
  inline Bool GetTristate() const { return this->data.GetTri(); }
  inline ColorPaletteData* Get(Bool force=false)
  {
    if (force) {
      ColorPaletteData* data = this->Get(false);
      if (!data) {
        this->data = GeData(CUSTOMDATATYPE_NR_COLORPALETTE, DEFAULTVALUE);
        CriticalAssert(!this->data.GetTri());
        data = this->Get(false);
        CriticalAssert(data && "CUSTOMDATATYPE_NR_COLORPALETTE could not be allocated");
      }
      return data;
    }
    else {
      if (this->data.GetTri()) return nullptr;
      return (ColorPaletteData*) this->data.GetValue().GetCustomDataType(CUSTOMDATATYPE_NR_COLORPALETTE);
    }
  }
};

struct ColorPaletteDataClass : CustomDataTypeClass
{
  static Int32 Flags()
  {
    return CUSTOMDATATYPE_INFO_HASSUBDESCRIPTION
      | CUSTOMDATATYPE_INFO_NEEDDATAFORSUBDESC
      | CUSTOMDATATYPE_INFO_DONTREGISTERGVTYPE
      | CUSTOMDATATYPE_INFO_SUBDESCRIPTIONDISABLEGUI
      | CUSTOMDATATYPE_INFO_UNDOSAMECUSTOMGUI
      | CUSTOMDATATYPE_INFO_LOADSAVE
      | CUSTOMDATATYPE_INFO_TOGGLEDISPLAY;
  }

public: // CustomDataTypeClass Overrides

  Int32 GetId()  override{ return CUSTOMDATATYPE_NR_COLORPALETTE; }
  CustomDataType* AllocData() override { return ColorPaletteData::Alloc(); }
  void FreeData(CustomDataType* data)  override{ ColorPaletteData::Free((ColorPaletteData*&) data); }
  Bool CopyData(CustomDataType const* src, CustomDataType* dst, AliasTrans* at) override
  {
    if (!src || !dst) return false;
    *(ColorPaletteData*) dst = *(ColorPaletteData const*) src;
    return true;
  }
  Int32 Compare(CustomDataType const* d1, CustomDataType const* d2) override
  {
    // We have no good way to compare the ColorPaletteData. Let's just
    // do it by pointers.
    if (d1 == d2) return 0;
    if (d1 < d2) return -1;
    return 1;
  }
  Bool WriteData(CustomDataType const* data, HyperFile* hf) override
  {
    if (!data || !hf) return false;
    return ((ColorPaletteData const*) data)->Write(hf);
  }
  Bool ReadData(CustomDataType* data, HyperFile* hf, Int32 disklevel) override
  {
    if (!data || !hf) return false;
    return ((ColorPaletteData*) data)->Read(hf, disklevel);
  }
  Char const* GetResourceSym() override { return "NR_COLORPALETTE"; }
  CustomProperty* GetProperties() override
  {
    static CustomProperty props[] = {
      {CUSTOMTYPE_LONG, NR_COLORPALETTE_SWATCH_SIZE, "SWATCH_SIZE"},
      {}
    };
    return props;
  }
  void GetDefaultProperties(BaseContainer& data) override
  {
    data.SetInt32(DESC_ANIMATE, DESC_ANIMATE_OFF);
    data.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_NR_COLORPALETTE);
    data.SetInt32(NR_COLORPALETTE_SWATCH_SIZE, 14);
  }
  Bool _GetDescription(CustomDataType const* data, Description& desc,
    DESCFLAGS_DESC& flags, BaseContainer const& parentdesc, DescID* sid) override;
  Bool GetParameter(CustomDataType const* data, DescID const& id,
    GeData& out, DESCFLAGS_GET& flags) override;
  Bool SetDParameter(CustomDataType* data, DescID const& id,
    GeData const& value, DESCFLAGS_SET& flags) override;
};

struct ColorPaletteUserArea : GeUserArea
{
  enum class InputState { None, Drag, Select, RangeSelect };

  static Int32 const padding = 1;
  static Bool  const show_folder_names = true;

  Int32 size = 14;
  ColorPaletteDataTristate* data = nullptr;  // Initialized by the dialog

  InputState istate = InputState::None;
  Int32 mousex = 0, mousey = 0;
  SwatchFolder* drag_target = nullptr;  // TODO: Reset to nullptr if iCustomGui::SetData() was called..?
  Int32 drag_target_index = NOTOK;
  Int32 range_select_source = NOTOK;

  inline Bool GetTristate() const { return this->data? this->data->GetTristate() :false; }

  inline ColorPaletteData* GetData(Bool force=false) const { return this->data? this->data->Get(force) :nullptr; }

public: // GeUserArea Overrides

  Bool GetMinSize(Int32& w, Int32& h) override
  {
    w = 128;  // Just choose some minimum width
    h = this->size;

    ColorPaletteData* data = this->GetData();
    if (data) {
      Int32 const rows = (Int32) data->folders.GetCount();
      h = this->size * rows;
      if (this->show_folder_names) {
        h += (this->DrawGetFontHeight() + this->padding * 2) * rows;
      }
      h += this->size;  // Extra space for another swatch row
    }
    return true;
  }

  Bool InputEvent(BaseContainer const& msg) override
  {
    ColorPaletteData* data = this->GetData();
    if (!data) return false;

    Int32 const device = msg.GetInt32(BFM_INPUT_DEVICE);
    Int32 const channel = msg.GetInt32(BFM_INPUT_CHANNEL);
    Int32 const qual = msg.GetInt32(BFM_INPUT_QUALIFIER);
    if (device == BFM_INPUT_MOUSE && channel == BFM_INPUT_MOUSELEFT) {
      data->SelectAll(false);
      this->istate = qual&QSHIFT? InputState::RangeSelect :InputState::Select;
      this->mousex = msg.GetInt32(BFM_INPUT_X);
      this->mousey = msg.GetInt32(BFM_INPUT_Y);
      this->Global2Local(&this->mousex, &this->mousey);
      this->Redraw();
      this->istate = InputState::None;

      BaseContainer parent_msg;

      // Was this is a double click? Rename the element that was clicked.
      Swatch* swatch = data->GetSelected();
      if (msg.GetBool(BFM_INPUT_DOUBLECLICK) && swatch) {
        String name = swatch->GetName();
        if (RenameDialog(&name)) {
          swatch->name = name;
          parent_msg.SetBool(GUIMSG_VALUE_CHANGED, true);
        }
      }

      // Tell the parent dialog that some things changed,
      // but that doesn't require a value update.
      parent_msg.SetBool(GUIMSG_SELECTION_CHANGED, true);
      nr::c4d::send_command(this, parent_msg);
      return true;
    }
    else if (device == BFM_INPUT_KEYBOARD) {
      BaseContainer parent_msg;
      switch (channel) {
        case 'A':
          if (qual & QCTRL) {
            data->SelectAll();
            this->Redraw();
            parent_msg.SetBool(GUIMSG_SELECTION_CHANGED, true);
          }
          break;
        case KEY_DELETE:
        case KEY_BACKSPACE:
          // TODO: As if selected should be deleted.
          data->DeleteSelected();
          parent_msg.SetBool(GUIMSG_VALUE_CHANGED, true);
          break;
      }
      if (parent_msg.GetIndexId(0) != NOTOK) {  // Container is not empty
        nr::c4d::send_command(this, parent_msg);
        return true;
      }
    }
    return false;
  }

  Int32 Message(BaseContainer const& msg, BaseContainer& res) override
  {
    switch (msg.GetId()) {
      case BFM_DRAGRECEIVE: {
        if (msg.GetInt32(BFM_DRAG_LOST)) {
          this->istate = InputState::None;
          this->Redraw();
          break;
        }

        Bool const drag_finished = msg.GetInt32(BFM_DRAG_FINISHED);
        ColorPaletteData* palette_data = this->GetData(drag_finished);
        if (!palette_data) return false;

        Int32 type = 0;
        void* data = nullptr;
        Bool accepted = false;
        this->GetDragObject(msg, &type, &data);
        maxon::BaseArray<Vector> colors;

        switch (type) {
          case DRAGTYPE_RGB:
            accepted = true;
            if (data) colors.Append(*reinterpret_cast<Vector*>(data));
            break;
          #if API_VERSION >= 17000
          case DRAGTYPE_RGB_ARRAY:
            // DRAGTYPE_RGB_ARRAY was added in R17.032
            accepted = true;
            if (data) colors.CopyFrom(*reinterpret_cast<maxon::BaseArray<Vector>*>(data));
            break;
          #endif
          default:
            SetDragDestination(MOUSE_FORBIDDEN);
            break;
        }

        if (accepted) {
          SetDragDestination(MOUSE_INSERTCOPY);
          this->istate = InputState::Drag;
          this->mousex = msg.GetInt32(BFM_DRAG_SCREENX);
          this->mousey = msg.GetInt32(BFM_DRAG_SCREENY);
          this->Screen2Local(&this->mousex, &this->mousey);
        }

        // Insert the the colors when the drag is finished.
        if (accepted && drag_finished) {
          CriticalAssert(palette_data != nullptr);
          this->istate = InputState::None;
          if (!this->drag_target) {
            this->drag_target = palette_data->AskNewFolder();
            this->drag_target_index = NOTOK;
          }
          if (this->drag_target) {
            for (Vector const& color : colors) {
              palette_data->NewSwatch(this->drag_target, color, this->drag_target_index);
              if (this->drag_target_index != NOTOK)
                ++this->drag_target_index;
            }
          }
          else accepted = false;
          if (accepted) {
            // Tell the parent dialog that the value changed.
            BaseContainer parent_msg;
            parent_msg.SetBool(GUIMSG_VALUE_CHANGED, true);
            nr::c4d::send_command(this, parent_msg);
          }
        }
        this->Redraw();
        return true;
      }
    }
    return GeUserArea::Message(msg, res);
  }

  void DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, BaseContainer const& msg) override
  {
    OffScreenOn();
    this->drag_target = nullptr;
    this->drag_target_index = NOTOK;

    Bool const tri = this->GetTristate();
    GeData bgcol = tri? COLOR_BGEDIT :COLOR_BG;
    DrawSetPen(bgcol);
    DrawRectangle(x1, y1, x2, y2);
    if (tri) return;

    ColorPaletteData* data = this->GetData();
    if (!data) return;

    // Get the folder icon.
    DrawSetTextCol(COLOR_TEXT, bgcol);
    IconData folder_icon;
    GetIcon(RESOURCEIMAGE_TIMELINE_FOLDER3, &folder_icon);

    // In this array we'll collect all selected swatches.
    struct rect { Swatch const* swatch; Int32 x1, y1; };
    maxon::BaseArray<rect> swatch_rects;
    Int32 range_select_end = NOTOK;  // The swatch the user clicked on during range selection
    Bool drag_painted = false;

    Int32 yoff = 0;
    Int const nfolders = data->folders.GetCount();
    for (Int32 idx = 0; idx < nfolders + 1; ++idx) {
      if (idx == nfolders && (drag_painted || this->istate != InputState::Drag))
        break;
      SwatchFolder* folder = nullptr;
      if (idx < nfolders)
        folder = &data->folders[idx];

      Int32 xoff = 0;
      Int32 miny = yoff;
      Int32 height = this->size;
      if (idx != 0) {
        if (this->show_folder_names) {
          DrawText(folder? folder->name :"New Folder...", 0, yoff + this->padding);  // TODO: Localization
          Int32 const lineh = DrawGetFontHeight() + this->padding * 2;
          height += lineh;
          yoff += lineh;
        }
        DrawSetPen(bgcol);
        nr::c4d::draw_icon(this, folder_icon, 0, yoff, this->size+1, this->size+1);
        DrawSetPen(COLOR_BGFOCUS);
        nr::c4d::draw_border(this, xoff, yoff, xoff+this->size, yoff+this->size);
        xoff = this->size;
      }

      // Check if any input event happens on this line.
      Bool line_target = false;
      switch (this->istate) {
        case InputState::Drag:
          if (idx == nfolders && !drag_painted)
            line_target = true;
          else
            line_target = (this->mousey >= miny && this->mousey < (miny + height));
          break;
        case InputState::Select:
        case InputState::RangeSelect:
          line_target = (this->mousey >= yoff && this->mousey < (yoff + this->size));
          break;
      }

      // Draw all swatches and 1 more (to be able to draw the drop indicator).
      Int const nswatches = folder? folder->swatches.GetCount() : 0;
      for (Int32 j = 0; j < nswatches + 1; ++j) {
        if (j < nswatches) {
          Swatch& swatch = folder->swatches[j];
          DrawSetPen(swatch.color);
          DrawRectangle(xoff, yoff, xoff+this->size-1, yoff+this->size-1);
          DrawSetPen(COLOR_BGFOCUS);
          nr::c4d::draw_border(this, xoff, yoff, xoff+this->size, yoff+this->size);
          swatch_rects.Append({&swatch, xoff, yoff});

          // Check if the user is clicking on the swatch.
          if (line_target && this->mousex >= xoff && this->mousex < xoff + this->size) {
            switch (this->istate) {
              case InputState::Select:
                swatch.selected = true;
                this->range_select_source = swatch.index;
                line_target = false;
                break;
              case InputState::RangeSelect:
                range_select_end = swatch.index;
                line_target = false;
                break;
            }
          }
        }
        if (line_target && this->istate == InputState::Drag) {
          // Draw the drop indicator to the left of the swatch
          // or after the last swatch.
          if (this->mousex < (xoff + this->size) || j == nswatches) {
            this->drag_target = folder;
            this->drag_target_index = j;
            DrawSetPen(COLOR_TEXTFOCUS);
            DrawRectangle(xoff-1, yoff, xoff+1, yoff+this->size);
            line_target = false;
            drag_painted = true;
          }
        }
        xoff += this->size;
      }
      yoff += this->size;
    }

    if (range_select_end != NOTOK) {
      data->SelectRange(this->range_select_source, range_select_end);
    }

    // Draw the selection rectangles.
    for (rect const& r : swatch_rects) {
      if (!r.swatch->selected) continue;
      DrawSetPen(COLOR_TEXTFOCUS);
      nr::c4d::draw_border(this, r.x1, r.y1, r.x1+this->size, r.y1+this->size);
      DrawSetPen(Vector());
      nr::c4d::draw_border(this, r.x1, r.y1, r.x1+this->size, r.y1+this->size, 1, 1);
    }
  }
};

class ColorPaletteCustomGui : public iCustomGui
{
  ColorPaletteUserArea ua;
  ColorPaletteDataTristate data;
  BitmapButtonCustomGui* bmp_show_chooser = nullptr;

public:

  ColorPaletteCustomGui(BaseContainer const& settings, CUSTOMGUIPLUGIN* plug)
    : iCustomGui(settings, plug)
  {
    this->ua.data = &this->data;
    this->ua.size = maxon::Max(settings.GetInt32(NR_COLORPALETTE_SWATCH_SIZE), 10);
  }

  void Update() {
    auto const* data = this->data.Get();

    Enable(GADGET_MAINGROUP, !this->data.GetTristate());
    HideElement(GADGET_COLORCHOOSER, data? !data->show_chooser :false);
    this->LayoutChanged(GADGET_MAINGROUP);
    this->ua.Redraw();

    if (data && this->bmp_show_chooser)
      this->bmp_show_chooser->SetToggleState(!data->show_chooser);

    // Show the name of the selected swatch.
    if (data) {
      Swatch const* swatch = data->GetSelected();
      this->SetString(GADGET_SWATCH_NAME, swatch? swatch->GetName() :"-");
      if (swatch) {
        this->SetColorField(GADGET_COLORCHOOSER, swatch->color, 1.0, 1.0, 0);
        g_last_color = swatch->color;
        g_last_brightness = 1.0;
      }
    }
  }

public: // iCustomGui Overrides

  Bool SetData(TriState<GeData> const& data) override
  {
    this->data = data;
    this->Update();
    return true;
  }

  TriState<GeData> GetData() override { return this->data; }

public: // GeDialog Overrides

  Bool CreateLayout() override
  {
    GroupBegin(GADGET_MAINGROUP, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, ""_s, 0); {
      GroupSpace(1, 1);
      GroupBorderSpace(2, 2, 2, 2);
      GroupBegin(0, BFH_SCALEFIT | BFV_SCALEFIT, 2, 0, ""_s, 0); {
        GroupBegin(0, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, ""_s, 0); {
          GroupBorderSpace(2, 2, 2, 2);
          GroupBorderNoTitle(BORDER_GROUP_IN);
          AddUserArea(GADGET_USERAREA, BFH_SCALEFIT | BFV_SCALEFIT);
          AttachUserArea(this->ua, GADGET_USERAREA);
        } GroupEnd();
        BaseContainer settings;
        if (g_last_layout) settings.SetContainer(CUSTOMGUI_SAVEDLAYOUTDATA, *g_last_layout);
        AddColorChooser(GADGET_COLORCHOOSER, BFH_SCALEFIT | BFV_SCALEFIT, 0, 0, 0, settings);
      } GroupEnd();
      GroupBegin(0, BFH_SCALEFIT, 0, 1, ""_s, 0); {
        AddStaticText(GADGET_SWATCH_NAME, BFH_SCALEFIT, 0, 0, "-"_s, BORDER_NONE);
        this->bmp_show_chooser = nr::c4d::add_bitmap_button(
          this, GADGET_SHOWCOLORCHOOSER, RESOURCEIMAGE_EYEACTIVE, RESOURCEIMAGE_EYEINACTIVE);
      } GroupEnd();
    } GroupEnd();
    this->Update();
    return true;
  }

  Bool InitValues() override
  {
    this->SetColorField(GADGET_COLORCHOOSER, g_last_color, g_last_brightness, 1.0, 0);
    return true;
  }

  Bool Command(Int32 param, BaseContainer const& bc) override
  {
    ColorPaletteData* data = nullptr;
    Swatch* swatch = nullptr;
    switch (param) {
      case GADGET_USERAREA:
        if (bc.GetBool(GUIMSG_VALUE_CHANGED))
          nr::c4d::send_value_changed(this, bc);
        if (bc.GetBool(GUIMSG_SELECTION_CHANGED))
          this->Update();
        break;
      case GADGET_SHOWCOLORCHOOSER:
        data = this->data.Get(true);
        if (data) {
          data->show_chooser = !data->show_chooser;
          nr::c4d::send_value_changed(this, bc);
          this->Update();
        }
        return true;
      case GADGET_COLORCHOOSER:
        // Save the last value of the color chooser globally.
        this->GetColorField(param, g_last_color, g_last_brightness);

        {
          if (!g_last_layout) {
            g_last_layout = NewObjClear(BaseContainer);
          }
          if (g_last_layout) {
            GeData temp = this->SendMessage(GADGET_COLORCHOOSER, BaseContainer(BFM_GETCUSTOMGUILAYOUTDATA));
            BaseContainer* layout = temp.GetContainer();
            if (layout) *g_last_layout = *layout;
            else g_last_layout->FlushAll();
          }
        }

        // If a swatch is selected, update its color and send the
        // value update.
        data = this->data.Get();
        if (data && (swatch = data->GetSelected())) {
          Float temp;
          this->GetColorField(param, swatch->color, temp);
          nr::c4d::send_value_changed(this, bc);
        }
        return true;
    }
    return false;
  }

  Bool CoreMessage(Int32 msg, BaseContainer const& bc) override
  {
    switch (msg) {
      case EVMSG_CHANGE:
        this->Update();
        break;
    }
    return iCustomGui::CoreMessage(msg, bc);
  }
};

struct ColorPaletteCustomGuiData
  : public nr::c4d::customgui_data<ColorPaletteCustomGui>
{
  ColorPaletteCustomGuiData() : super(CUSTOMGUI_NR_COLORPALETTE, "NR_COLORPALETTE") { }
};

// ===========================================================================

Bool ColorPaletteData::Write(HyperFile* hf) const
{
  if (!hf->WriteInt32(this->next_idx)) return false;
  if (!hf->WriteBool(this->show_chooser)) return false;
  if (!hf->WriteInt32((Int32) this->folders.GetCount())) return false;
  for (auto& folder : this->folders) {
    if (!folder.Write(hf)) return false;
  }
  return true;
}

Bool ColorPaletteData::Read(HyperFile* hf, Int32 disklevel)
{
  if (!hf->ReadInt32(&this->next_idx)) return false;
  if (!hf->ReadBool(&this->show_chooser)) return false;
  this->folders.Flush();
  Int32 count;
  if (!hf->ReadInt32(&count)) return false;
  iferr (this->folders.EnsureCapacity(count)) return false;
  for (Int32 i = 0; i < count; ++i) {
    SwatchFolder folder;
    if (!folder.Read(hf, disklevel)) return false;
    this->folders.Append(std::move(folder));
  }
  return true;
}

Bool ColorPaletteDataClass::_GetDescription(
  CustomDataType const* _data, Description& desc, DESCFLAGS_DESC& flags,
  BaseContainer const& parentdesc, DescID* sid)
{
  auto* data = static_cast<ColorPaletteData const*>(_data);
  if (!data) return false;

  // Helper Function
  auto ExportSwatch = [&desc](Swatch const& swatch) {
    DescID did(DescLevel(swatch.index, DTYPE_COLOR, 0));
    BaseContainer param = GetCustomDataTypeDefault(DTYPE_COLOR);
    param.SetString(DESC_NAME, swatch.GetName());
    param.SetString(DESC_SHORT_NAME, swatch.GetName());
    desc.SetParameter(did, param, DESCID_ROOT);
  };

  if (sid != nullptr) {
    Int32 const index = (*sid)[0].id;
    auto* swatch = dynamic_cast<Swatch const*>(data->Find(index));
    if (swatch) {
      ExportSwatch(*swatch);
    }
    flags |= DESCFLAGS_DESC_LOADED;
  }
  else {
    for (auto& folder : data->folders) {
      for (auto& swatch : folder.swatches) {
        ExportSwatch(swatch);
      }
    }
    flags |= DESCFLAGS_DESC_LOADED;
  }

  return CustomDataTypeClass::_GetDescription(data, desc, flags, parentdesc, sid);
}

Bool ColorPaletteDataClass::GetParameter(
  CustomDataType const* _data, DescID const& id, GeData& out, DESCFLAGS_GET& flags)
{
  auto* data = static_cast<ColorPaletteData const*>(_data);
  if (!data) return false;

  Int32 const index = id[0].id;
  auto* swatch = dynamic_cast<Swatch const*>(data->Find(index));
  if (swatch) {
    out = GeData(swatch->color);
    flags |= DESCFLAGS_GET_PARAM_GET;
    return true;
  }

  return CustomDataTypeClass::GetParameter(data, id, out, flags);
}

Bool ColorPaletteDataClass::SetDParameter(
  CustomDataType* _data, DescID const& id, GeData const& value, DESCFLAGS_SET& flags)
{
  auto* data = static_cast<ColorPaletteData*>(_data);
  if (!data) return false;

  Int32 const index = id[0].id;
  auto* swatch = dynamic_cast<Swatch*>(data->Find(index));
  if (swatch && (value.GetType() == DTYPE_COLOR || value.GetType() == DTYPE_VECTOR)) {
    swatch->color = value.GetVector();
    flags |= DESCFLAGS_SET_PARAM_SET;
    return true;
  }

  return CustomDataTypeClass::SetDParameter(data, id, value, flags);
}

Bool RegisterColorPalette()
{
  static Int32 const disklevel = 0;
  if (!nr::c4d::register_customgui_plugin("Color Palette", 0, NewObjClear(ColorPaletteCustomGuiData)))
    return false;
  if (!RegisterCustomDataTypePlugin("Color Palette"_s, ColorPaletteDataClass::Flags(), NewObjClear(ColorPaletteDataClass), disklevel))
    return false;

  return true;
}
