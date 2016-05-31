/* Copyright (C) 2015, Niklas Rosenstein
 * All rights reserved.
 *
 * @file source/procedural/Channel/DrawHelper.h
 */

#ifndef PROCEDURAL_DRAWHELPER_H__
#define PROCEDURAL_DRAWHELPER_H__

#include <c4d.h>
#include <lib_clipmap.h>
#include <nr/c4d/viewport.h>
#include <nr/procedural/channel.h>

using nr::procedural::Channel;
using nr::c4d::MatrixDraw;

//----------------------------------------------------------------------------
/// This class provides the interface for representing Channel values in
/// the Cinema 4D viewport.
//----------------------------------------------------------------------------
class I_ChannelDrawHelper
{
public:
  struct DrawData
  {
    Channel* channel;
    BaseObject* op;
    BaseDraw* bd;
    BaseDrawHelp* bh;
    Bool enabled[PROCEDURAL_CHANNEL_MAXITEMLENGTH];
    Vector colors[PROCEDURAL_CHANNEL_MAXITEMLENGTH];
  };

  virtual ~I_ChannelDrawHelper() { }
  virtual Bool Init(DrawData& data) = 0;
  virtual void Draw(DrawData const& data, Int32 index, Int32 subIndex, GeData const& value) = 0;
  virtual void Free(DrawData const& data) { }

};

//----------------------------------------------------------------------------
/// This implementation of the I_ChannelDrawHelper interface draws the
/// items into world or local object space. Does not support string values.
//----------------------------------------------------------------------------
class LWChannelDrawHelper : public I_ChannelDrawHelper
{
public:
  LWChannelDrawHelper(Bool isLocal)
    : mIsLocal(isLocal), mType(NOTOK), mMatDraw() { }
  virtual Bool Init(DrawData& data) override;
  virtual void Draw(DrawData const& data, Int32 index, Int32 subIndex, GeData const& value) override;
private:
  Bool mIsLocal;
  Int32 mType;
  MatrixDraw mMatDraw;
};

//----------------------------------------------------------------------------
/// This I_ChannelDrawHelper draws the channel information based on reference
/// positions in local/global space (eg. object points, polygon centers or
/// the points of another channel).
//----------------------------------------------------------------------------
class RefChannelDrawHelper : public I_ChannelDrawHelper
{
public:
  RefChannelDrawHelper(Bool visualMode)
    : mVisualMode(visualMode), mType(NOTOK), mRefType(NOTOK),
      mRefItemLength(0), mTranslate(), mMatDraw(), mMap(), mRef(nullptr) { }

  virtual Bool Init(DrawData& data) override;
  virtual void Draw(DrawData const& data, Int32 index, Int32 subIndex, GeData const& value) override;
private:
  Bool mVisualMode;
  Int32 mType;
  Int32 mRefType;
  Int32 mRefItemLength;
  Matrix mTranslate;
  MatrixDraw mMatDraw;
  AutoAlloc<GeClipMap> mMap;
  Channel* mRef;
};

#endif // PROCEDURAL_DRAWHELPER_H__
