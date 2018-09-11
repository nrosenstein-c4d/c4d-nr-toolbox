/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file procedural/Channel/Channel.cpp
/// \lastmodified 2015/07/10

#define NR_LOGGING_PREFIX "Procedural|Channel.cpp"

#include <c4d_apibridge.h>
#include "Channel.h"
#include "ChannelApiHook.h"
#include "DrawHelper.h"
#include "../DynamicArray.h"
#include "misc/print.h"

#include <nr/procedural/channel.h>
#include <NiklasRosenstein/c4d/bitmaps.hpp>        // nr::c4d::tint_image
#include <NiklasRosenstein/c4d/description.hpp>    // nr::c4d::show_parameter
#include <NiklasRosenstein/c4d/raii.hpp>           // nr::raii::auto_bitmap
#include <NiklasRosenstein/c4d/string.hpp>         // nr::c4d::tostr
#include <NiklasRosenstein/macros.hpp>             // NR_IF
#include <NiklasRosenstein/math.hpp>          // nr::math::clamp

namespace nr { using namespace niklasrosenstein; }

using c4d_apibridge::IsEmpty;
using c4d_apibridge::GetDescriptionID;
using nr::c4d::tostr;
using nr::clamp;
using nr::procedural::Channel;
using nr::procedural::dynamicarray::DynamicArray;

/// **************************************************************************
/// This structure is a container for global data to avoid multiple global
/// variables. It contains the icons for the Procedural Channel plugin which
/// are loaded upon \fn nr::procedural::RegisterChannelPlugin().
/// **************************************************************************
static struct {
  BaseBitmap* database;
  BaseBitmap* databaseError;
  BaseBitmap* databaseLock;
} G = { nullptr, nullptr, nullptr };

/// **************************************************************************
/// **************************************************************************
static Vector g_defaultColors[PROCEDURAL_CHANNEL_MAXITEMLENGTH] = {
  Vector(255 / 255.,  76 / 255.,  76 / 255.),
  Vector(255 / 255., 165 / 255.,  76 / 255.),
  Vector(255 / 255., 255 / 255.,  76 / 255.),
  Vector(165 / 255., 255 / 255.,  76 / 255.),
  Vector( 76 / 255., 255 / 255.,  76 / 255.),
  Vector( 76 / 255., 255 / 255., 165 / 255.),
  Vector( 76 / 255., 255 / 255., 255 / 255.),
  Vector( 76 / 255., 165 / 255., 255 / 255.),
  Vector(198 / 255.,   0 / 255.,   0 / 255.),
  Vector(198 / 255.,  99 / 255.,   0 / 255.),
  Vector(198 / 255., 198 / 255.,   0 / 255.),
  Vector( 99 / 255., 198 / 255.,   0 / 255.),
  Vector(  0 / 255., 198 / 255.,   0 / 255.),
  Vector(  0 / 255., 198 / 255.,  99 / 255.),
  Vector(  0 / 255., 198 / 255., 198 / 255.),
  Vector(  0 / 255.,  99 / 255., 198 / 255.),
};


/// **************************************************************************
/// Helper function to assign a value, returns \c true if a new value was
/// assigned, \c false if the value didn't change.
/// **************************************************************************
template <typename T>
Bool CheckAssign(T& dest, const T& v) {
  if (dest != v) {
    dest = v;
    return true;
  }
  return false;
}

/// **************************************************************************
/// Used for ChannelPlugin::UpdateChannelData().
/// **************************************************************************
enum CHANNELUPDATE
{
  CHANNELUPDATE_NONE,
  CHANNELUPDATE_ONCHANGE,
  CHANNELUPDATE_FORCE,
};

/// **************************************************************************
/// This class implements the internals of a Procedural channel. It can be
/// easily interfaced with using the nr::procedural::Channel class.
/// **************************************************************************
class ChannelPlugin : public TagData
{
  typedef TagData SUPER;
public:
  ChannelPlugin();
  virtual ~ChannelPlugin();

  /// Computes the element offset index based on the current frame
  /// number of the channel.
  Int32 GetFrameOffsetIndex() const
  {
    CriticalAssert(mFrameCount >= 1);
    Int32 frame = mFrame % mFrameCount;
    if (frame < 0)
      frame += mFrameCount;
    return frame * mCount * mItemLength;
  }

  /// Computes a linear index from a DescID that attempts to access
  /// a sub-element of the channel. Returns a negative value if the DescID
  /// is invalid or out of range.
  Int32 GetIndexFromDescID(const DescID& id) const
  {
    CriticalAssert(mItemLength >= 1 && mItemLength <= PROCEDURAL_CHANNEL_MAXITEMLENGTH);
    const Int32 depth = id.GetDepth();
    if (depth > 3) return -1;
    Int32 elementIndex = (depth >= 2 ? id[1].id - 1 : 0);  // There can not be a DescLevel with id zero,
    Int32 itemIndex = (depth >= 3 ? id[2].id - 1 : 0);     // we expect the index to be offset by 1.
    if (elementIndex < 0 || elementIndex >= mCount) return -1;
    if (itemIndex < 0 || itemIndex >= mItemLength) return -1;
    return elementIndex * mItemLength + itemIndex;
  }

  /// Fills the GeData with the value of an element from the
  /// internal DynamicArray based on the current channel type.
  /// @param index The index of the element.
  /// @param data The data to fill.
  /// @return \c true if the data was filled, \c false if not (eg. if
  ///     the index is out of bounds).
  Bool GetElement(Int32 index, GeData& data) const;
  inline Bool GetElement(const DescID& id, GeData& data, Bool frameOffset) const
  {
    Int32 index = GetIndexFromDescID(id);
    if (frameOffset) {
      if (index < 0 || index >= mCount * mItemLength)
        return false;
      index += this->GetFrameOffsetIndex();
    }
    if (index >= 0) return GetElement(index, data);
    return false;
  }

  /// Sets an element in the DynamicArray based on the current
  /// channel type from the specified GeData object.
  /// @param index The index of the element.
  /// @param data The data to read from.
  /// @return \c true if the element was set, \c false if not (eg. if
  ///     the index is out of bounds).
  Bool SetElement(Int32 index, const GeData& data) const;
  inline Bool SetElement(const DescID& id, const GeData& data, Bool frameOffset) const
  {
    Int32 index = GetIndexFromDescID(id);
    if (frameOffset) {
      if (index < 0 || index >= mCount * mItemLength)
        return false;
      index += this->GetFrameOffsetIndex();
    }
    if (index >= 0) return SetElement(index, data);
    return false;
  }

  /// Flushes the memory of the channel, resets its element count to
  /// zero and its datatype to \var PROCEDURAL_CHANNEL_TYPE_NIL.
  /// @param resetAttribs If \c true, \var mCount and \var mType will
  ///     be reset to zero values.
  void FlushChannel(Bool resetAttribs=true);

  /// Resizes the internal array.
  void UpdateChannelData(CHANNELUPDATE updateType=CHANNELUPDATE_ONCHANGE);

  /// Computes the custom icon for this channel. The returned bitmap
  /// is owned by the channel.
  BaseBitmap* GetCustomIcon(Channel* channel);

  /// DataAllocator for plugin registration.
  static NodeData* Alloc() { return NewObjClear(ChannelPlugin); }

  /// \addtogroup TagData Overrides
  /// @{
  virtual EXECUTIONRESULT Execute(
    BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt,
    Int32 priority, EXECUTIONFLAGS flags) override;
  virtual Bool AddToExecution(BaseTag* tag, PriorityList* list) override;
  virtual Bool Draw(
    BaseTag* tag, BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh) override;
  /// @}

  /// \addtogroup NodeData Overrides
  /// @{
  virtual Bool Init(GeListNode* node) override;
  virtual Bool GetDParameter(
    GeListNode* node, const DescID& id,
    GeData& data, DESCFLAGS_GET& flags) override;
  virtual Bool SetDParameter(
    GeListNode* node, const DescID& id,
    const GeData& data, DESCFLAGS_SET& flags) override;
  virtual Bool GetDDescription(
    GeListNode* node, Description* desc, DESCFLAGS_DESC& flags) override;
  virtual Bool GetDEnabling(
    GeListNode* node, const DescID& id, const GeData& data,
    DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) override;
  virtual Bool Message(GeListNode* node, Int32 type, void* pData) override;
  virtual Bool Read(GeListNode* node, HyperFile* hf, Int32 disklevel) override;
  virtual Bool Write(GeListNode* node, HyperFile* hf) override;
  virtual Bool CopyTo(
    NodeData* dest, GeListNode* snode, GeListNode* dnode,
    COPYFLAGS flags, AliasTrans* at) override;
  /// @}

private:
  /// \c true if the channel is locked and should ignore all write actions.
  Bool mLocked;
  /// PROCEDURAL_CHANNEL_DIRTYCOUNT
  Int32 mDirtyCount;
  /// PROCEDURAL_CHANNEL_MODE
  Int32 mMode;
  /// PROCEDURAL_CHANNEL_TYPE
  Int32 mType;
  /// The number of elements in the channel.
  Int32 mCount;
  /// The number of items per element.
  Int32 mItemLength;
  /// The number of frames to allocate.
  Int32 mFrameCount;
  /// The current frame number.
  Int32 mFrame;
  /// True if the frame number should be synchronized with the document frame.
  Bool mSyncFrame;
  /// The document frame offset.
  Int32 mFrameOffset;
  /// PROCEDURAL_CHANNEL_STATE
  Int32 mState;
  /// The reference channel from which the count is used.
  String mRef;
  /// The actual data.
  DynamicArray<> mData;
  /// The datatype that the #mData was recently allocated with.
  Int32 mDataPrevType;
  /// Flag used to prevent infinite recursion when broadcasting count changes.
  Bool mInBroadcast;
  /// BaseBitmap that contains a tinted version of the icon.
  BaseBitmap* mIcon;
  /// Dirtycount for the icon bitmap generated from the channel state
  /// and the R, G and B components of the tint color.
  Int32 mIconDirty;
};

/// **************************************************************************
/// **************************************************************************
ChannelPlugin::ChannelPlugin() : mData()
{
  mLocked = false;
  mDirtyCount = 0;
  mMode = PROCEDURAL_CHANNEL_MODE_FLOATING;
  mType = PROCEDURAL_CHANNEL_TYPE_NIL;
  mDataPrevType = PROCEDURAL_CHANNEL_TYPE_NIL;
  mCount = 0;
  mItemLength = 1;
  mFrameCount = 1;
  mFrame = 0;
  mSyncFrame = true;
  mFrameOffset = 0;
  mState = PROCEDURAL_CHANNEL_STATE_UNINITIALIZED;
  mRef = "";
  mInBroadcast = false;
  mIcon = nullptr;
  mIconDirty = -1;
}

/// **************************************************************************
/// **************************************************************************
ChannelPlugin::~ChannelPlugin()
{
  FlushChannel();
  CriticalAssert(mData.IsEmpty());
  mRef = "";
  BaseBitmap::Free(mIcon);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::GetElement(Int32 index, GeData& data) const
{
  if (!mData.Accessible(index))
    return false;
  switch (mDataPrevType) { // switch: PROCEDURAL_CHANNEL_TYPE
    case PROCEDURAL_CHANNEL_TYPE_INTEGER:
      data.SetInt32(mData.Get<Int32>()[index]);
      return true;
    case PROCEDURAL_CHANNEL_TYPE_FLOAT:
      data.SetFloat(mData.Get<Float>()[index]);
      return true;
    case PROCEDURAL_CHANNEL_TYPE_VECTOR:
      data.SetVector(mData.Get<Vector>()[index]);
      return true;
    case PROCEDURAL_CHANNEL_TYPE_MATRIX:
      data.SetMatrix(mData.Get<Matrix>()[index]);
      return true;
    case PROCEDURAL_CHANNEL_TYPE_STRING:
      data.SetString(mData.Get<String>()[index]);
      return true;
    case PROCEDURAL_CHANNEL_TYPE_NIL:
    default:
      break;
  }
  return false;
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::SetElement(Int32 index, const GeData& data) const
{
  if (!mData.Accessible(index))
    return false;
  switch (mDataPrevType) { // switch: PROCEDURAL_CHANNEL_TYPE
    case PROCEDURAL_CHANNEL_TYPE_INTEGER:
      mData.Get<Int32>()[index] = data.GetInt32();
      return true;
    case PROCEDURAL_CHANNEL_TYPE_FLOAT:
      mData.Get<Float>()[index] = data.GetFloat();
      return true;
    case PROCEDURAL_CHANNEL_TYPE_VECTOR:
      mData.Get<Vector>()[index] = data.GetVector();
      return true;
    case PROCEDURAL_CHANNEL_TYPE_MATRIX:
      mData.Get<Matrix>()[index] = data.GetMatrix();
      return true;
    case PROCEDURAL_CHANNEL_TYPE_STRING:
      mData.Get<String>()[index] = data.GetString();
      return true;
    case PROCEDURAL_CHANNEL_TYPE_NIL:
    default:
      break;
  }
  return false;
}

/// **************************************************************************
/// **************************************************************************
void ChannelPlugin::FlushChannel(Bool resetAttribs)
{
  switch (mDataPrevType) { // switch: PROCEDURAL_CHANNEL_TYPE
    case PROCEDURAL_CHANNEL_TYPE_INTEGER:
      mData.Destroy<Int32>();
      break;
    case PROCEDURAL_CHANNEL_TYPE_FLOAT:
      mData.Destroy<Float>();
      break;
    case PROCEDURAL_CHANNEL_TYPE_VECTOR:
      mData.Destroy<Vector>();
      break;
    case PROCEDURAL_CHANNEL_TYPE_MATRIX:
      mData.Destroy<Matrix>();
      break;
    case PROCEDURAL_CHANNEL_TYPE_STRING:
      mData.Destroy<String>();
      break;
    case PROCEDURAL_CHANNEL_TYPE_NIL:
    default:
      break;
  }
  if (resetAttribs) {
    mType = PROCEDURAL_CHANNEL_TYPE_NIL;
    mCount = 0;
  }
  mDataPrevType = PROCEDURAL_CHANNEL_TYPE_NIL;
  mState = PROCEDURAL_CHANNEL_STATE_UNINITIALIZED;
  CriticalAssert(mData.IsEmpty());
}

/// **************************************************************************
/// **************************************************************************
void ChannelPlugin::UpdateChannelData(CHANNELUPDATE updateType)
{
  // Retrieve the channel and the BaseContainer that contains the
  // default values for our channel types.
  Channel* channel = static_cast<Channel*>(Get());
  BaseContainer* bc = (channel ? channel->GetDataInstance() : nullptr);

  Bool changed = false;
  if (updateType == CHANNELUPDATE_FORCE)
    changed = true;

  // If either could not be retrieved, that's an unknown error. Send
  // a broadcast message if the channel was not already in that state.
  if (!channel || !bc) {
    FlushChannel(false);
    if (changed || mState != PROCEDURAL_CHANNEL_STATE_UNKNOWN) {
      mState = PROCEDURAL_CHANNEL_STATE_UNKNOWN;
      mDirtyCount++;
      channel->BroadcastMessage(MSG_PROCEDURAL_CHANNEL_BROADCASTUPDATE, channel);
    }
    return;
  }

  // If a new type is assigned to this channel, we need to flush all
  // existing data and assign the new type.
  if (mType != mDataPrevType) {
    FlushChannel(false);
    mDataPrevType = mType;
    changed = true;
  }

  // Read the count from the reference channel if it exists.
  Bool success = true;
  Int32 newState = PROCEDURAL_CHANNEL_STATE_UNKNOWN;
  NR_IF (!IsEmpty(mRef)) {
    // Find the reference chanenl and retrieved its data object.
    Channel* ref = channel->Find(mRef);
    ChannelPlugin* data = nullptr;
    if (ref) {
      data = ref->GetNodeData<ChannelPlugin>();
    }

    // If neither could be retrieved, that's an error. Send a broadcast
    // update if the channel was not already in that state.
    if (!data || data->mState != PROCEDURAL_CHANNEL_STATE_INITIALIZED) {
      mCount = 0;
      newState = PROCEDURAL_CHANNEL_STATE_REFERROR;
      changed = true;
      success = false;
      break;
    }

    if (data->mCount != mCount) {
      mCount = data->mCount;
      changed = true;
    }
  }

  if (success) {
    CriticalAssert(mCount >= 0);
    CriticalAssert(mFrameCount >= 0);
    CriticalAssert(mItemLength >= 1 && mItemLength <= PROCEDURAL_CHANNEL_MAXITEMLENGTH);
    Int32 const targetSize = mCount * mItemLength * mFrameCount;

    // Resize the DynamicArray.
    CriticalAssert(mType == mDataPrevType);
    switch (mType) { // switch: PROCEDURAL_CHANNEL_TYPE
      case PROCEDURAL_CHANNEL_TYPE_INTEGER:
        success = mData.Resize<Int32>(targetSize,
          bc->GetInt32(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_INTEGER));
        break;
      case PROCEDURAL_CHANNEL_TYPE_FLOAT:
        success = mData.Resize<Float>(targetSize,
          bc->GetFloat(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_FLOAT));
        break;
      case PROCEDURAL_CHANNEL_TYPE_VECTOR:
        success = mData.Resize<Vector>(targetSize,
          bc->GetVector(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_VECTOR));
        break;
      case PROCEDURAL_CHANNEL_TYPE_MATRIX:
        success = mData.Resize<Matrix>(targetSize,
          bc->GetMatrix(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_MATRIX));
        break;
      case PROCEDURAL_CHANNEL_TYPE_STRING:
        success = mData.Resize<String>(targetSize,
          bc->GetString(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_STRING));
        break;
      case PROCEDURAL_CHANNEL_TYPE_NIL:
        newState = PROCEDURAL_CHANNEL_STATE_UNINITIALIZED;
        break;
      default:
        newState = PROCEDURAL_CHANNEL_STATE_TYPEERROR;
        break;
    }
  }

  if (!success && newState == PROCEDURAL_CHANNEL_STATE_UNKNOWN)
    newState = PROCEDURAL_CHANNEL_STATE_MEMORYERROR;
  else if (success)
    newState = PROCEDURAL_CHANNEL_STATE_INITIALIZED;
  if (mState != newState) {
    mState = newState;
    changed = true;
  }
  if (changed) mDirtyCount++;
  if (changed && updateType != CHANNELUPDATE_NONE) {
    channel->BroadcastMessage(MSG_PROCEDURAL_CHANNEL_BROADCASTUPDATE, channel);
  }
}

/// **************************************************************************
/// **************************************************************************
BaseBitmap* ChannelPlugin::GetCustomIcon(Channel* channel)
{
  BaseContainer* bc = channel->GetDataInstance();
  if (!bc) return nullptr;
  Bool useTint = bc->GetBool(PROCEDURAL_CHANNEL_UI_TINTENABLED);
  Vector tintColor = bc->GetVector(PROCEDURAL_CHANNEL_UI_TINTCOLOR);
  BaseBitmap* bmp = G.database;
  if (mState != PROCEDURAL_CHANNEL_STATE_UNINITIALIZED &&
      mState != PROCEDURAL_CHANNEL_STATE_INITIALIZED)
  {
    bmp = G.databaseError;
    useTint = false;
  }
  else if (mLocked) {
    bmp = G.databaseLock;
  }
  if (!useTint || !bmp) {
    BaseBitmap::Free(mIcon);
    return bmp;
  }

  Int32 r, g, b;
  nr::c4d::calc_rgb<8>(tintColor, &r, &g, &b);
  Int32 dirty = r + g + b + mState;
  if (mIcon && dirty == mIconDirty)
    return mIcon;

  BaseBitmap::Free(mIcon);
  mIcon = bmp->GetClone();
  if (!mIcon) return nullptr;
  nr::c4d::tint(mIcon, tintColor);
  return mIcon;
}

/// **************************************************************************
/// **************************************************************************
EXECUTIONRESULT ChannelPlugin::Execute(
  BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt,
  Int32 priority, EXECUTIONFLAGS flags)
{
  if (priority == EXECUTIONPRIORITY_INITIAL && mSyncFrame)
    mFrame = doc->GetTime().GetFrame(doc->GetFps());
  return EXECUTIONRESULT_OK;
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::AddToExecution(BaseTag* tag, PriorityList* list)
{
  list->Add(tag, EXECUTIONPRIORITY_INITIAL, EXECUTIONFLAGS_0);
  return true;
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::Draw(
  BaseTag* tag, BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh)
{
  auto* channel = static_cast<Channel*>(tag);
  BaseContainer* bc = (tag ? tag->GetDataInstance() : nullptr);
  if (!bd || !bh || !op || !bc)
    return false;

  CriticalAssert(mItemLength >= 0 && mItemLength < PROCEDURAL_CHANNEL_MAXITEMLENGTH);

  // Allocate the draw helper that will do the viewport drawing.
  I_ChannelDrawHelper* drawHelper = nullptr;
  Int32 const mode = bc->GetInt32(PROCEDURAL_CHANNEL_DISPLAY_MODE);
  switch (mode) {
    case PROCEDURAL_CHANNEL_DISPLAY_MODE_LOCAL:
    case PROCEDURAL_CHANNEL_DISPLAY_MODE_GLOBAL:
      drawHelper = NewObjClear(LWChannelDrawHelper, mode == PROCEDURAL_CHANNEL_DISPLAY_MODE_LOCAL);
      break;
    case PROCEDURAL_CHANNEL_DISPLAY_MODE_REFTEXT:
    case PROCEDURAL_CHANNEL_DISPLAY_MODE_REFVIS:
      drawHelper = NewObjClear(RefChannelDrawHelper, mode == PROCEDURAL_CHANNEL_DISPLAY_MODE_REFVIS);
    default:
      break;
  }

  // Initialize the draw data and retreive the channel display information.
  I_ChannelDrawHelper::DrawData drawData;
  drawData.channel = channel;
  drawData.op = op;
  drawData.bd = bd;
  drawData.bh = bh;
  if (!channel->GetDisplayColors(drawData.enabled, drawData.colors)) {
    print::error("Could not read channel display colors.");
    return false;
  }

  // Invoke the draw helper.
  if (drawHelper && drawHelper->Init(drawData)) {
    Int32 const offset = this->GetFrameOffsetIndex();
    for (Int32 subIndex = 0; subIndex < mItemLength; ++subIndex) {
      if (!drawData.enabled[subIndex])
        continue;
      bd->SetPen(drawData.colors[subIndex]);
      for (Int32 index = 0; index < mCount; ++index) {
        GeData value;
        this->GetElement(offset + index * mItemLength + subIndex, value);
        drawHelper->Draw(drawData, index, subIndex, value);
      }
    }
  }
  if (drawHelper) {
    drawHelper->Free(drawData);
    DeleteObj(drawHelper);
  }

  return true;
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::Init(GeListNode* node)
{
  if (!node || !SUPER::Init(node))
    return false;
  BaseContainer* bc = static_cast<Channel*>(node)->GetDataInstance();
  if (!bc)
    return false;

  bc->SetBool(PROCEDURAL_CHANNEL_UI_TINTENABLED, false);
  bc->SetVector(PROCEDURAL_CHANNEL_UI_TINTCOLOR, Vector(1.0));

  // switch: PROCEDURAL_CHANNEL_TYPE
  bc->SetInt32(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_INTEGER, 0);
  bc->SetFloat(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_FLOAT, 0.0);
  bc->SetVector(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_VECTOR, Vector(0, 0, 0));
  bc->SetMatrix(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_MATRIX, Matrix());
  bc->SetString(PROCEDURAL_CHANNEL_PARAM_DEFAULTV_STRING, ""_s);

  bc->SetInt32(PROCEDURAL_CHANNEL_DISPLAY_MODE, PROCEDURAL_CHANNEL_DISPLAY_MODE_OFF);

  // Initialize the display colors.
  for (Int32 index = 0; index < PROCEDURAL_CHANNEL_MAXITEMLENGTH; ++index) {
    Int32 const id = PROCEDURAL_CHANNEL_DISPLAY_COLOR_START +
      index * PROCEDURAL_CHANNEL_DISPLAY_COLOR_SLOTS;
    bc->SetBool(id + 0, true);
    bc->SetVector(id + 1, g_defaultColors[index]);
  }

  UpdateChannelData();
  return true;
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::GetDParameter(
  GeListNode* node, const DescID& id, GeData& data, DESCFLAGS_GET& flags)
{
  switch (id[0].id) {
    case PROCEDURAL_CHANNEL_DIRTYCOUNT:
      data.SetInt32(mDirtyCount);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_MODE:
      data.SetInt32(mMode);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_TYPE:
      data.SetInt32(mDataPrevType);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_COUNT:
      data.SetInt32(mCount);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_ITEMLENGTH:
      data.SetInt32(mItemLength);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_FRAMECOUNT:
      data.SetInt32(mFrameCount);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_FRAME:
      data.SetInt32(mFrame);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_SYNCFRAME:
      data.SetInt32(mSyncFrame);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_FRAMEOFFSET:
      data.SetInt32(mFrameOffset);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_STATE:
      data.SetInt32(mState);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_HANDLE:
      data.SetVoid(mData.GetDataHandle());
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_REF:
      data.SetString(mRef);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_LOCKED:
      data.SetInt32(mLocked);
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_DATA:
      // If this parmaeter is accessed, a second description ID level
      // must be present representing the index of the element to be
      // accessed. Subtract by one because a zero DescLevel does not exist.
      if (GetElement(id, data, true)) {
        flags |= DESCFLAGS_GET_PARAM_GET;
        return true;
      }
      else {
        const String name = static_cast<BaseTag*>(node)->GetName();
        print::error("[" + name + "]: Get PARAM_DATA: invalid DescID " + tostr(id));
      }
      return false;
    case PROCEDURAL_CHANNEL_UI_MEMINFO:
      data.SetString(mData.ToString());
      flags |= DESCFLAGS_GET_PARAM_GET;
      return true;
  }
  return SUPER::GetDParameter(node, id, data, flags);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::SetDParameter(
  GeListNode* node, const DescID& id,
  const GeData& data, DESCFLAGS_SET& flags)
{
  switch (id[0].id) {
    case PROCEDURAL_CHANNEL_PARAM_MODE:
      if (mLocked) return false;
      mMode = data.GetInt32();
      flags |= DESCFLAGS_SET_PARAM_SET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_TYPE:
      if (mLocked) return false;
      mType = data.GetInt32();
      UpdateChannelData();
      flags |= DESCFLAGS_SET_PARAM_SET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_COUNT:
      if (mLocked) return false;
      if (CheckAssign(mCount, maxon::Max(0, data.GetInt32())))
        UpdateChannelData(CHANNELUPDATE_FORCE);
      flags |= DESCFLAGS_SET_PARAM_SET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_ITEMLENGTH: {
      if (mLocked) return false;
      Int32 value = clamp<Int32>(data.GetInt32(), 1,
        PROCEDURAL_CHANNEL_MAXITEMLENGTH);
      if (CheckAssign(mItemLength, value))
        UpdateChannelData(CHANNELUPDATE_FORCE);
      flags |= DESCFLAGS_SET_PARAM_SET;
      return true;
    }
    case PROCEDURAL_CHANNEL_PARAM_FRAMECOUNT:
      if (mLocked) return false;
      if (CheckAssign(mFrameCount, maxon::Max(1, data.GetInt32())))
        UpdateChannelData(CHANNELUPDATE_FORCE);
      flags |= DESCFLAGS_SET_PARAM_SET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_FRAME:
      if (mSyncFrame) return false;
      mFrame = data.GetInt32();
      flags |= DESCFLAGS_SET_PARAM_SET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_SYNCFRAME:
      mSyncFrame = data.GetBool();
      flags |= DESCFLAGS_SET_PARAM_SET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_FRAMEOFFSET:
      mFrameOffset = data.GetInt32();
      flags |= DESCFLAGS_SET_PARAM_SET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_REF:
      if (mLocked) return false;
      if (CheckAssign(mRef, data.GetString()))
        UpdateChannelData(CHANNELUPDATE_FORCE);
      flags |= DESCFLAGS_SET_PARAM_SET;
      return true;
    case PROCEDURAL_CHANNEL_PARAM_LOCKED:
      if (mMode == PROCEDURAL_CHANNEL_MODE_PERMANENT) {
        mLocked = data.GetBool();
        flags |= DESCFLAGS_SET_PARAM_SET;
        return true;
      }
      return false;
    case PROCEDURAL_CHANNEL_PARAM_HANDLE:
      return false;
    case PROCEDURAL_CHANNEL_PARAM_DATA:
      if (mLocked) return false;
      if (SetElement(id, data, true)) {
        flags |= DESCFLAGS_SET_PARAM_SET;
        return true;
      }
      else {
        const String name = static_cast<BaseTag*>(node)->GetName();
        print::error("[" + name + "]: Set PARAM_DATA: invalid DescID " + tostr(id));
      }
      return false;
  }
  return SUPER::SetDParameter(node, id, data, flags);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::GetDEnabling(
  GeListNode* node, const DescID& id, const GeData& data,
  DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc)
{
  if (!node)
    return false;
  BaseContainer* bc = static_cast<BaseList2D*>(node)->GetDataInstance();
  if (!bc)
    return false;

  switch (id[0].id) {
    case PROCEDURAL_CHANNEL_PARAM_MODE:
    case PROCEDURAL_CHANNEL_PARAM_TYPE:
    case PROCEDURAL_CHANNEL_PARAM_REF:
    case PROCEDURAL_CHANNEL_UI_MANUALREINIT:
      return !mLocked;
    case PROCEDURAL_CHANNEL_PARAM_FRAME:
      return !mSyncFrame;
    case PROCEDURAL_CHANNEL_PARAM_FRAMEOFFSET:
      return mSyncFrame;
    case PROCEDURAL_CHANNEL_PARAM_STATE:
      // The "State" parameter should always be disabled, it
      // serves only as information to the user.
      return false;
    case PROCEDURAL_CHANNEL_PARAM_ITEMLENGTH:
    case PROCEDURAL_CHANNEL_PARAM_FRAMECOUNT:
    case PROCEDURAL_CHANNEL_PARAM_COUNT:
      if (mLocked) return false;
      return IsEmpty(mRef) && mType != PROCEDURAL_CHANNEL_TYPE_NIL;
    case PROCEDURAL_CHANNEL_UI_GOTOREF:
      return !IsEmpty(mRef) && static_cast<Channel*>(node)->Find(mRef);
    default:
      break;
  }
  return SUPER::GetDEnabling(node, id, data, flags, itemdesc);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::GetDDescription(
  GeListNode* node, Description* desc, DESCFLAGS_DESC& flags)
{
  using nr::c4d::show_parameter;

  if (!node || !desc || !desc->LoadDescription(node->GetType()))
    return false;
  flags |= DESCFLAGS_DESC_LOADED;

  show_parameter(desc, PROCEDURAL_CHANNEL_PARAM_LOCKED,
    mMode == PROCEDURAL_CHANNEL_MODE_PERMANENT);

  // switch: PROCEDURAL_CHANNEL_TYPE
  show_parameter(desc, PROCEDURAL_CHANNEL_PARAM_DEFAULTV_INTEGER,
    mDataPrevType == PROCEDURAL_CHANNEL_TYPE_INTEGER);
  show_parameter(desc, PROCEDURAL_CHANNEL_PARAM_DEFAULTV_FLOAT,
    mDataPrevType == PROCEDURAL_CHANNEL_TYPE_FLOAT);
  show_parameter(desc, PROCEDURAL_CHANNEL_PARAM_DEFAULTV_VECTOR,
    mDataPrevType == PROCEDURAL_CHANNEL_TYPE_VECTOR);
  show_parameter(desc, PROCEDURAL_CHANNEL_PARAM_DEFAULTV_MATRIX,
    mDataPrevType == PROCEDURAL_CHANNEL_TYPE_MATRIX);
  show_parameter(desc, PROCEDURAL_CHANNEL_PARAM_DEFAULTV_STRING,
    mDataPrevType == PROCEDURAL_CHANNEL_TYPE_STRING);

  // Generate display color parameters based on the item length.
  {
    DescID const* sid = desc->GetSingleDescID();
    DescID parentId = DescLevel(PROCEDURAL_CHANNEL_GROUP_DISPLAY_COLORS, DTYPE_GROUP, 0);
    DescID did;

    BaseContainer chkBc = GetCustomDataTypeDefault(DTYPE_BOOL);
    BaseContainer colBc = GetCustomDataTypeDefault(DTYPE_COLOR);
    for (Int32 index = 0; index < mItemLength; ++index) {
      Int32 const id = PROCEDURAL_CHANNEL_DISPLAY_COLOR_START +
        index * PROCEDURAL_CHANNEL_DISPLAY_COLOR_SLOTS;

      did = DescLevel(id + 0, DTYPE_BOOL, 0);
      if (!sid || did.IsPartOf(*sid, nullptr)) {
        chkBc.SetString(DESC_NAME, "Color." + tostr(index));
        desc->SetParameter(did, chkBc, parentId);
      }

      did = DescLevel(id + 1, DTYPE_COLOR, 0);
      if (!sid || did.IsPartOf(*sid, nullptr)) {
        colBc.SetString(DESC_NAME, ""_s);
        desc->SetParameter(did, colBc, parentId);
      }
    }
  }

  return SUPER::GetDDescription(node, desc, flags);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::Message(GeListNode* node, Int32 type, void* pData)
{
  if (!node)
    return false;
  Channel* op = static_cast<Channel*>(node);

  switch (type) {
    // ***********************************************************************
    case MSG_CHANGE:
      mDirtyCount++;
      return true;

    // ***********************************************************************
    case MSG_PROCEDURAL_CHANNEL_BROADCASTUPDATE:
      if (!mInBroadcast) {
        mInBroadcast = true;
        UpdateChannelData();
        mInBroadcast = false;
      }
      return true;
    // ***********************************************************************
    case MSG_PROCEDURAL_CHANNEL_REINIT:
      // TODO: Maybe it would be better to not use Destroy(), Resize() but
      // instead manually initialize all elements?
      if (!mLocked) {
        FlushChannel(false);
        UpdateChannelData();
      }
      return true;
    // ***********************************************************************
    case MSG_DESCRIPTION_COMMAND: {
      const auto* data = reinterpret_cast<const DescriptionCommand*>(pData);
      if (!data) break;
      if (GetDescriptionID(data) == PROCEDURAL_CHANNEL_UI_MANUALREINIT) {
        node->Message(MSG_PROCEDURAL_CHANNEL_REINIT);
        return true;
      }
      else if (GetDescriptionID(data) == PROCEDURAL_CHANNEL_UI_GOTOREF) {
        Channel* ref = reinterpret_cast<Channel*>(node)->Find(mRef);
        BaseDocument* doc = node->GetDocument();
        if (ref && doc) {
          doc->SetActiveTag(ref);
          EventAdd();
        }
        return true;
      }
      break;
    }
    // ***********************************************************************
    case MSG_GETCUSTOMICON: {
      auto* data = reinterpret_cast<GetCustomIconData*>(pData);
      if (!data) break;
      BaseBitmap* bmp = GetCustomIcon(static_cast<Channel*>(node));
      if (bmp) {
        *data->dat = IconData(bmp, 0, 0, bmp->GetBw(), bmp->GetBh());
        data->filled = true;
      }
      return true;
    }
    // ***********************************************************************
    case MSG_DESCRIPTION_GETINLINEOBJECT: {
      auto* data = reinterpret_cast<DescriptionInlineObjectMsg*>(pData);
      if (!data || !data->objects || !data->id) return false;
      const Int32 widgetId = (*data->id)[-1].id;

      if (widgetId == PROCEDURAL_CHANNEL_PARAM_REF) {
        Channel* ref = op->Find(mRef);
        if (ref) data->objects->Append(ref);
      }
      break;
    }
    default:
      break;
  }
  return SUPER::Message(node, type, pData);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::Read(GeListNode* node, HyperFile* hf, Int32 disklevel)
{
  if (!hf->ReadInt32(&mMode)) return false;
  if (!hf->ReadInt32(&mType)) return false;
  if (!hf->ReadInt32(&mCount)) return false;
  if (!hf->ReadInt32(&mItemLength)) return false;
  if (!hf->ReadInt32(&mFrameCount)) return false;
  if (!hf->ReadInt32(&mFrame)) return false;
  if (!hf->ReadBool(&mSyncFrame)) return false;
  if (!hf->ReadInt32(&mFrameOffset)) return false;
  if (!hf->ReadString(&mRef)) return false;
  mItemLength = clamp<Int32>(mItemLength, 1, PROCEDURAL_CHANNEL_MAXITEMLENGTH);
  UpdateChannelData();
  if (mMode == PROCEDURAL_CHANNEL_MODE_PERMANENT) {
    GeData data;
    for (Int32 index = 0; index < mCount * mItemLength * mFrameCount; ++index) {
      if (!hf->ReadGeData(&data)) return false;
      SetElement(index, data);
    }
  }
  return SUPER::Read(node, hf, disklevel);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::Write(GeListNode* node, HyperFile* hf)
{
  if (!hf->WriteInt32(mMode)) return false;
  if (!hf->WriteInt32(mDataPrevType)) return false;
  if (!hf->WriteInt32(mCount)) return false;
  if (!hf->WriteInt32(mItemLength)) return false;
  if (!hf->WriteInt32(mFrameCount)) return false;
  if (!hf->WriteInt32(mFrame)) return false;
  if (!hf->WriteBool(mSyncFrame)) return false;
  if (!hf->WriteInt32(mFrameOffset)) return false;
  if (!hf->WriteString(mRef)) return false;
  if (mMode == PROCEDURAL_CHANNEL_MODE_PERMANENT) {
    GeData data;
    for (Int32 index = 0; index < mCount * mItemLength * mFrameCount; ++index) {
      if (GetElement(index, data))
        hf->WriteGeData(data);
      else
        hf->WriteGeData(GeData());
    }
  }
  return SUPER::Write(node, hf);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelPlugin::CopyTo(
  NodeData* ndest, GeListNode* snode, GeListNode* dnode,
  COPYFLAGS flags, AliasTrans* at)
{
  if (!ndest || !snode || !dnode) return false;
  if (dnode->GetType() != snode->GetType()) return false;
  ChannelPlugin* dest = static_cast<ChannelPlugin*>(ndest);
  dest->mLocked = mLocked;
  dest->mMode = mMode;
  dest->mType = mDataPrevType;
  dest->mCount = mCount;
  dest->mItemLength = mItemLength;
  dest->mFrameCount = mFrameCount;
  dest->mFrame = mFrame;
  dest->mSyncFrame = mSyncFrame;
  dest->mFrameOffset = mFrameOffset;
  dest->mRef = mRef;
  dest->mDataPrevType = PROCEDURAL_CHANNEL_TYPE_NIL;
  dest->UpdateChannelData(CHANNELUPDATE_NONE);
  GeData data;
  for (Int32 index = 0; index < mCount * mItemLength * mFrameCount; ++index) {
    if (!GetElement(index, data)) break;
    if (!dest->SetElement(index, data)) break;
  }
  return SUPER::CopyTo(ndest, snode, dnode, flags, at);
}

/// **************************************************************************
/// **************************************************************************
Bool nr::procedural::RegisterChannelPlugin()
{
  if (!ChannelApiHook::Register()) {
    print::error("[NR Procedural - Error]: Channel API Hook could not be registered.");
    return false;
  }
  using nr::c4d::auto_bitmap;
  G.database = auto_bitmap("res/icons/channel_normal.png").release();
  G.databaseError = auto_bitmap("res/icons/channel_error.png").release();
  G.databaseLock = auto_bitmap("res/icons/channel_locked.png").release();
  const Int32 flags = TAG_VISIBLE | TAG_MULTIPLE | TAG_EXPRESSION;
  const Int32 disklevel = 0;
  if (!RegisterTagPlugin(
      PROCEDURAL_CHANNEL_ID,
      "Channel"_s,
      flags,
      ChannelPlugin::Alloc,
      "nrprocedural_channel"_s,
      G.database,
      disklevel))
    return false;
  return true;
}

/// **************************************************************************
/// **************************************************************************
void nr::procedural::UnloadChannelPlugin()
{
  BaseBitmap::Free(G.database);
  BaseBitmap::Free(G.databaseError);
  BaseBitmap::Free(G.databaseLock);
}
