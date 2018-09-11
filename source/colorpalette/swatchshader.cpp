/* Copyright (c) 2016 Niklas Rosenstein
 * All rights reserved. */

#include <c4d.h>
#include <c4d_apibridge.h>
#include <NiklasRosenstein/macros.hpp>
#include <NiklasRosenstein/c4d/utils.hpp>
#include "customgui_nrcolorpalette.h"
#include "res/description/Hnrtoolbox.h"
#include "res/description/Xnrswatch.h"

using niklasrosenstein::c4d::get_param;
using niklasrosenstein::c4d::set_param;

class SwatchShader : public ShaderData
{
  typedef ShaderData super;

  Vector render_color;

  inline String GetShaderName(BaseShader* shader)
  {
    String name = "???";
    BaseDocument* doc = shader->GetDocument();
    NR_IF (doc) {
      GeData temp;
      nr::ColorPaletteData* palette = nr::GetGlobalColorPalette(doc, temp);
      if (!palette) break;
      Int32 const id = get_param(shader, XNRSWATCH_ID).GetInt32();
      auto* swatch = dynamic_cast<nr::Swatch const*>(palette->Find(id));
      if (!swatch) break;
      name = swatch->GetName();
    }
    return name;
  }

public:

  static NodeData* Alloc() { return NewObjClear(SwatchShader); }

  // ShaderData Overrides

  virtual INITRENDERRESULT InitRender(
    BaseShader* shader, InitRenderStruct const& irs) override
  {
    this->render_color = Vector();

    GeData temp;
    auto* palette = nr::GetGlobalColorPalette(irs.doc, temp);
    if (palette) {
      Int32 const id = get_param(shader, XNRSWATCH_ID).GetInt32();
      auto* swatch = dynamic_cast<nr::Swatch const*>(palette->Find(id));
      if (swatch) {
        this->render_color = swatch->color;
      }
    }
    return INITRENDERRESULT_OK;
  }

  virtual Vector Output(BaseShader* shader, ChannelData* cd) override
  {
    return this->render_color;
  }

  // NodeData Overrides

  virtual Bool Init(GeListNode* node) override
  {
    BaseShader* shader = static_cast<BaseShader*>(node);
    BaseContainer* data = shader? shader->GetDataInstance() :nullptr;
    if (!shader || !data) {
      return false;
    }
    data->SetBool(XNRSWATCH_ID, 0);
    return super::Init(node);
  }

  virtual Bool GetDDescription(
    GeListNode* node, Description* desc, DESCFLAGS_DESC& flags) override
  {
    BaseShader* shader = static_cast<BaseShader*>(node);
    BaseContainer* bc = nullptr;
    if (!node || !desc || !desc->LoadDescription(node->GetType())) {
      return false;
    }
    flags |= DESCFLAGS_DESC_LOADED;

    bc = desc->GetParameterI(XNRSWATCH_NAME, nullptr);
    if (bc) {
      String name = this->GetShaderName(shader);
      bc->SetString(DESC_NAME, name);
      bc->SetString(DESC_SHORT_NAME, name);
    }

    return super::GetDDescription(node, desc, flags);
  }

  virtual Bool Message(GeListNode* node, Int32 msg, void* pdata) override
  {
    switch (msg) {
      case MSG_DESCRIPTION_POPUP: {
        auto* doc = node->GetDocument();
        auto* data = reinterpret_cast<DescriptionPopup*>(pdata);
        if (!doc || !data) return false;
        if (c4d_apibridge::GetDescriptionID(data) == XNRSWATCH_SELECT) {
          if (c4d_apibridge::GetDescriptionChosen(data) == 0) {
            GeData temp;
            auto* palette = nr::GetGlobalColorPalette(doc, temp);
            palette->FillPopupContainer(c4d_apibridge::GetDescriptionPopup(data));
          }
          else {
            set_param(node, XNRSWATCH_ID, c4d_apibridge::GetDescriptionChosen(data));
          }
        }
        return true;
      }
    }
    return super::Message(node, msg, pdata);
  }

};

Bool RegisterSwatchShader()
{
  Int32 const info = 0;
  Int32 const dlevel = 0;
  return RegisterShaderPlugin(
    Xnrswatch,
    "nrSwatch"_s,
    info,
    SwatchShader::Alloc,
    "Xnrswatch"_s,
    dlevel);
}
