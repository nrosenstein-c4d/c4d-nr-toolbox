/* Copyright (C) 2015, Niklas Rosenstein
 * All rights reserved.
 *
 * @file source/procedural/Channel/DrawHelper.cpp
 */

#define PRINT_PREFIX "[nr-toolbox/Procedural|DrawHelper.cpp]: "

#include <nr/math/math.h>
#include <nr/c4d/string.h>
#include <nr/c4d/viewport.h>

#include "DrawHelper.h"
#include "misc/print.h"
#include "res/description/nrprocedural_channel.h"

using nr::math::clamp;
using nr::c4d::draw_text;
using nr::c4d::tostr;

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
Bool LWChannelDrawHelper::Init(DrawData& data)
{
  if (mIsLocal)
    data.bd->SetMatrix_Matrix(data.op, data.op->GetMg());
  else
    data.bd->SetMatrix_Matrix(nullptr, Matrix());
  mType = data.channel->GetChannelType();
  return true;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void LWChannelDrawHelper::Draw(
  DrawData const& data, Int32 index, Int32 subIndex, GeData const& value)
{
  switch (mType) {
    case PROCEDURAL_CHANNEL_TYPE_INTEGER:
      data.bd->DrawHandle(Vector(0, value.GetInt32(), 0), DRAWHANDLE_SMALL, 0);
      break;
    case PROCEDURAL_CHANNEL_TYPE_FLOAT:
      data.bd->DrawHandle(Vector(0, value.GetFloat(), 0), DRAWHANDLE_SMALL, 0);
      break;
    case PROCEDURAL_CHANNEL_TYPE_VECTOR:
      data.bd->DrawHandle(value.GetVector(), DRAWHANDLE_SMALL, 0);
      break;
    case PROCEDURAL_CHANNEL_TYPE_MATRIX: {
      Matrix const mat = value.GetMatrix();
      data.bd->DrawHandle(mat.off, DRAWHANDLE_SMALL, 0);
      mMatDraw(data.bd, mat, 1.0);
      data.bd->SetPen(data.colors[subIndex]);
      break;
    }
    default:
      break;
  }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
Bool RefChannelDrawHelper::Init(DrawData& data)
{
  if (!mMap)
    return false;  // memory error

  mRef = data.channel->Find(data.channel->GetRefChannelName());
  if (!mRef) {
    print::error("No reference channel.");
    return false;
  }
  mType = data.channel->GetChannelType();
  mRefType = mRef->GetChannelType();
  mRefItemLength = mRef->GetItemLength();
  if (data.channel->GetCount() != mRef->GetCount()) {
    print::error("Reference channel count doesn't match channel count");
    return false;
  }

  // Check if we can handle the reference channel type.
  switch (mRefType) {
    case PROCEDURAL_CHANNEL_TYPE_INTEGER:
    case PROCEDURAL_CHANNEL_TYPE_FLOAT:
    case PROCEDURAL_CHANNEL_TYPE_VECTOR:
    case PROCEDURAL_CHANNEL_TYPE_MATRIX:
      break;
    default:
      print::error("Can only render on Integer, Float, Vector or Matrix reference channel.");
      return false;
  }

  // Disable item iterations for items that the reference channel doesnt have.
  for (Int32 index = mRefItemLength; index < PROCEDURAL_CHANNEL_MAXITEMLENGTH; ++index) {
    data.enabled[index] = false;
  }

  // Choose the translation matrix based on the reference channels display mode.
  mTranslate = Matrix();
  if (mRef->GetDisplayMode() != PROCEDURAL_CHANNEL_DISPLAY_MODE_GLOBAL)
    mTranslate = data.op->GetMg();

  // Set the BaseDraw matrix based on wether we will display the data
  // in text or visual mode.
  if (mVisualMode) {
    switch (mType) {
      case PROCEDURAL_CHANNEL_TYPE_INTEGER:
      case PROCEDURAL_CHANNEL_TYPE_FLOAT:
      case PROCEDURAL_CHANNEL_TYPE_VECTOR:
      case PROCEDURAL_CHANNEL_TYPE_MATRIX:
        data.bd->SetMatrix_Matrix(data.op, mTranslate);
        break;
      default:
        data.bd->SetMatrix_Screen();
        break;
    }
  }
  else {
    data.bd->SetMatrix_Screen();
  }

  return true;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void RefChannelDrawHelper::Draw(
  DrawData const& data, Int32 index, Int32 subIndex, GeData const& value)
{
  // We've disabled all item channels that the reference channel doesn't support.
  CriticalAssert(subIndex >= 0 && subIndex < mRefItemLength);

  GeData ref;
  if (!mRef->GetElement(ref, index, subIndex)) {
    print::error("Could not read Element (" + tostr(index) +
      ", " + tostr(subIndex) + ") from reference channel.");
    return;
  }

  // Figure the position at which the text should be displayed.
  Vector offset;
  switch (mRefType) {
    case PROCEDURAL_CHANNEL_TYPE_INTEGER:
      offset = Vector(0, ref.GetInt32(), 0);
      break;
    case PROCEDURAL_CHANNEL_TYPE_FLOAT:
      offset = Vector(0, ref.GetFloat(), 0);
      break;
    case PROCEDURAL_CHANNEL_TYPE_VECTOR:
      offset = ref.GetVector();
      break;
    case PROCEDURAL_CHANNEL_TYPE_MATRIX:
      offset = ref.GetMatrix().off;
      break;
    default:
      break;
  }


  if (mVisualMode) {
    switch (mType) {
      case PROCEDURAL_CHANNEL_TYPE_INTEGER:
        data.bd->DrawLine(offset, offset + Vector(0, value.GetInt32(), 0), 0);
        return;
      case PROCEDURAL_CHANNEL_TYPE_FLOAT:
        data.bd->DrawLine(offset, offset + Vector(0, value.GetFloat(), 0), 0);
        return;
      case PROCEDURAL_CHANNEL_TYPE_VECTOR:
        data.bd->DrawLine(offset, offset + value.GetVector(), 0);
        return;
      case PROCEDURAL_CHANNEL_TYPE_MATRIX: {
        Matrix mat = value.GetMatrix();
        mat.off += offset;
        data.bd->DrawLine(offset, mat.off, 0);
        mMatDraw(data.bd, mat, 1.0);
        data.bd->SetPen(data.colors[subIndex]);
        return;
      }
      default:
        break;
    }
  }

  // Figure the text to display.
  String text;
  switch (mType) {
    case PROCEDURAL_CHANNEL_TYPE_INTEGER:
      text = tostr(value.GetInt32());
      break;
    case PROCEDURAL_CHANNEL_TYPE_FLOAT:
      text = tostr(value.GetFloat());
      break;
    case PROCEDURAL_CHANNEL_TYPE_VECTOR:
      text = tostr(value.GetVector());
      break;
    case PROCEDURAL_CHANNEL_TYPE_MATRIX:
      text = tostr(value.GetMatrix());
      break;
    case PROCEDURAL_CHANNEL_TYPE_STRING:
      text = value.GetString();
      break;
    default:
      break;
  }

  if (!text.Content())
    return;

  offset = mTranslate * offset;
  Vector ws = data.bd->WS(offset);
  draw_text(data.bd, mMap, text, ws.x, ws.y, 4, data.colors[subIndex], Vector(0));
}
