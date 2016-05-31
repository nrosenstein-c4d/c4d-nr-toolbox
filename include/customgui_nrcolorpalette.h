// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#pragma once
#include <c4d.h>
#include "res/description/Hnrtoolbox.h"

enum
{
  CUSTOMDATATYPE_NR_COLORPALETTE = 1037808,
  CUSTOMGUI_NR_COLORPALETTE = 1037809,
  NR_COLORPALETTE_SWATCH_SIZE = 1000,  // LONG
};

namespace nr {

struct SwatchBase
{
  Int32 index = NOTOK; // Constant and unique swatch index
  String name;         // Swatch (or folder) name
  SwatchBase() { }
  SwatchBase(Int32 index, String const& name) : index(index), name(name) { }
  SwatchBase(SwatchBase&& that) : index(that.index), name(std::move(that.name)) { }
  SwatchBase(SwatchBase const& that) : index(that.index), name(that.name) { }
  virtual ~SwatchBase() { }
  virtual Bool Write(HyperFile* hf) const
  {
    if (!hf->WriteInt32(this->index)) return false;
    if (!hf->WriteString(this->name)) return false;
    return true;
  }
  virtual Bool Read(HyperFile* hf, Int32 disklevel)
  {
    if (!hf->ReadInt32(&this->index)) return false;
    if (!hf->ReadString(&this->name)) return false;
    return true;
  }
  virtual String GetName() const { return this->name; }
};

struct Swatch : SwatchBase
{
  Vector color;  // Color value
  Bool selected = false;  // Swatch selection state (not saved)
  Swatch() : SwatchBase(), color() { }
  Swatch(Int32 index, String const& name, Vector const& color)
    : SwatchBase(index, name), color(color) { }

  // TODO: Why are these not generated implicitly?
  Swatch(Swatch const& that) : SwatchBase(that), color(that.color), selected(that.selected) { }
  Swatch(Swatch&& that) : SwatchBase(that), color(std::move(that.color)), selected(that.selected) { }
  MAXON_OPERATOR_MOVE_ASSIGNMENT(Swatch);
  virtual Bool Write(HyperFile* hf) const override
  {
    if (!SwatchBase::Write(hf)) return false;
    if (!hf->WriteVector(this->color)) return false;
    return true;
  }
  virtual Bool Read(HyperFile* hf, Int32 disklevel) override
  {
    if (!SwatchBase::Read(hf, disklevel)) return false;
    if (!hf->ReadVector(&this->color)) return false;
    return true;
  }
  virtual String GetName() const override
  {
    if (!this->name.Content())
      return "Swatch#" + String::IntToString(this->index);
    return this->name;
  }
};

struct SwatchFolder : SwatchBase
{
  maxon::BaseArray<Swatch> swatches;  // List of child swatches
  SwatchFolder() : SwatchBase(), swatches() { }
  SwatchFolder(Int32 index, String const& name) : SwatchBase(index, name) { }
  SwatchFolder(SwatchFolder const& that)
    : SwatchBase(that) { swatches.CopyFrom(that.swatches); }
  NR_OPERATOR_COPY_ASSIGNMENT(SwatchFolder);
  virtual Bool Write(HyperFile* hf) const override
  {
    if (!SwatchBase::Write(hf)) return false;
    if (!hf->WriteInt32((Int32) this->swatches.GetCount())) return false;
    for (auto& swatch : this->swatches)
      if (!swatch.Write(hf)) return false;
    return true;
  }
  virtual Bool Read(HyperFile* hf, Int32 disklevel) override
  {
    if (!SwatchBase::Read(hf, disklevel)) return false;
    Int32 count;
    if (!hf->ReadInt32(&count)) return false;
    this->swatches.Flush();
    if (!this->swatches.EnsureCapacity(count)) return false;
    for (Int32 i = 0; i < count; ++i) {
      Swatch swatch;
      if (!swatch.Read(hf, disklevel)) return false;
      this->swatches.Append(std::move(swatch));
    }
    return true;
  }
};

class ColorPaletteData : public iCustomDataType<ColorPaletteData>
{
  Int32 next_idx = 0;
public:
  Bool show_chooser = true;
  maxon::BaseArray<SwatchFolder> folders;

  ColorPaletteData() { folders.Append({this->GetNextIndex(), "<root>"}); }

  ColorPaletteData(ColorPaletteData const& that)
    : next_idx(that.next_idx), show_chooser(that.show_chooser)
  {
    folders.CopyFrom(that.folders);
  }

  NR_OPERATOR_COPY_ASSIGNMENT(ColorPaletteData);

  Bool Write(HyperFile* hf) const;

  Bool Read(HyperFile* hf, Int32 disklevel);

  inline Int32 GetNextIndex() { return this->next_idx++; }

  inline SwatchBase* Find(Int32 id) { return const_cast<SwatchBase*>(NR_MAKE_CONST(this)->Find(id)); }

  inline SwatchBase const* Find(Int32 id) const
  {
    for (auto& folder : this->folders) {
      if (folder.index == id) return &folder;
      for (auto& swatch : folder.swatches) {
        if (swatch.index == id) return &swatch;
      }
    }
    return nullptr;
  }

  inline Swatch* Find(String const& name) { return const_cast<Swatch*>(NR_MAKE_CONST(this)->Find(name)); }

  inline Swatch const* Find(String const& name) const
  {
    for (auto& folder : this->folders) {
      for (auto& swatch : folder.swatches) {
        if (swatch.GetName() == name) return &swatch;
      }
    }
    return nullptr;
  }

  inline Swatch* GetSelected() { return const_cast<Swatch*>(NR_MAKE_CONST(this)->GetSelected()); }

  inline Swatch const* GetSelected() const
  {
    Swatch const* result = nullptr;
    for (auto& folder : this->folders) {
      for (auto& swatch : folder.swatches) {
        if (swatch.selected) {
          if (result) return nullptr;  // multiple swatches selected
          result = &swatch;
        }
      }
    }
    return result;
  }

  inline SwatchFolder* NewFolder(String const& name)
  {
    return this->folders.Append(SwatchFolder{this->GetNextIndex(), name});
  }

  inline SwatchFolder* AskNewFolder()
  {
    String name;
    if (RenameDialog(&name))
      return this->NewFolder(name);
    return nullptr;
  }

  inline Swatch* NewSwatch(SwatchFolder* folder, Vector const& color, Int32 index=NOTOK)
  {
    if (!folder) return nullptr;
    Swatch* res = index==NOTOK? folder->swatches.Append() :folder->swatches.Insert(index);
    if (res) {
      res->index = this->GetNextIndex();
      res->color = color;
    }
    return res;
  }

  inline void SelectAll(Bool state=true)
  {
    for (auto& folder : this->folders) {
      for (auto& swatch : folder.swatches)
        swatch.selected = state;
    }
  }

  inline void SelectRange(Int32 start_index, Int32 end_index)
  {
    Bool opened = false;
    for (auto& folder : this->folders) {
      for (auto& swatch : folder.swatches) {
        Bool const matches = (swatch.index == start_index || swatch.index == end_index);
        if (matches) {
          swatch.selected = true;
          if (!opened) opened = true;
          else return;
        }
        else swatch.selected = opened;
      }
    }
  }

  inline void DeleteSelected()
  {
    for (auto& folder : this->folders) {
      FilterArray(folder.swatches, [](Swatch const& swatch) {
        return !swatch.selected;
      });
    }

    // Move empty folders to the end of the array, then downsize.
    Bool first = true;
    FilterArray(this->folders, [&first](SwatchFolder const& folder) {
      if (first) { // Keep the root swatch folder
        first = false;
        return true;
      }
      return folder.swatches.GetCount() > 0;
    });
  }

  inline void FillPopupContainer(BaseContainer& popup)
  {
    if (!this->folders.GetCount()) return;
    for (Int i = 0; i < this->folders.GetCount(); ++i) {
      auto& folder = this->folders[i];
      BaseContainer submenu;
      submenu.InsData(1, folder.name);
      BaseContainer* dest = i==0? &popup :&submenu;
      for (auto& swatch : folder.swatches) {
        dest->InsData(swatch.index, swatch.GetName());
      }
      if (i != 0)
        popup.InsData(0, submenu);
    }
  }

public: // static members

  inline static ColorPaletteData* Get(GeData& data)
  {
    return static_cast<ColorPaletteData*>(data.GetCustomDataType(CUSTOMDATATYPE_NR_COLORPALETTE));
  }

  inline static ColorPaletteData const* Get(GeData const& data)
  {
    return static_cast<ColorPaletteData const*>(data.GetCustomDataType(CUSTOMDATATYPE_NR_COLORPALETTE));
  }

  inline static BaseContainer* GetPrefs()
  {
    BaseContainer* bc = GetWorldPluginData(CUSTOMDATATYPE_NR_COLORPALETTE);
    if (!bc) {
      SetWorldPluginData(CUSTOMDATATYPE_NR_COLORPALETTE, BaseContainer());
      bc = GetWorldPluginData(CUSTOMDATATYPE_NR_COLORPALETTE);
    }
    return bc;
  }

  template <typename ARRAYTYPE, typename FUNC>
  void FilterArray(ARRAYTYPE& array, FUNC&& func)
  {
    auto count = array.GetCount();
    // Move the elements that are to be removed to the end of the array.
    for (decltype(count) i = 0; i < count; ++i) {
      if (!func(array[i])) {
        std::swap(array[i--], array[--count]);
      }
    }
    // Then downsize.
    array.Resize(count);
  }
};

inline ColorPaletteData* GetGlobalColorPalette(BaseDocument* doc, GeData& data)
{
  if (!doc) return nullptr;
  ColorPaletteData* result = nullptr;
  BaseSceneHook* hook = doc->FindSceneHook(Hnrtoolbox);
  if (hook) {
    hook->GetParameter(NRTOOLBOX_HOOK_SWATCHES, data, DESCFLAGS_GET_0);
    result = ColorPaletteData::Get(data);
  }
  return result;
}

inline Bool SetGlobalColorPalette(BaseDocument* doc, ColorPaletteData const* data)
{
  if (!doc) return false;
  GeData value(CUSTOMGUI_NR_COLORPALETTE, DEFAULTVALUE);
  ColorPaletteData* detail = ColorPaletteData::Get(value);
  BaseSceneHook* hook = doc->FindSceneHook(Hnrtoolbox);
  if (detail && data && hook) {
    *detail = *data;
    return hook->SetParameter(NRTOOLBOX_HOOK_SWATCHES, value, DESCFLAGS_SET_0);
  }
  return false;
}

} // namespace nr
