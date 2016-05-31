/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file procedural/channel.h
/// \lastmodified 2015/11/03

#ifndef PROCEDURAL_CHANNEL_H_
#define PROCEDURAL_CHANNEL_H_

#include <c4d.h>
#include "res/description/nrprocedural_channel.h"

/// **************************************************************************
/// **************************************************************************
enum
{
  /// This message can be sent to re-initialize the chanenl with its
  /// default values. The message will be ignored if the channel is
  /// permanent.
  MSG_PROCEDURAL_CHANNEL_REINIT = 1035297,

  /// This message is sent to all neighbouring channels from one channel
  /// if its count is changed. Channels that refer to this channel must
  /// update their own count.
  MSG_PROCEDURAL_CHANNEL_BROADCASTUPDATE = 1035298,
};

namespace nr {
namespace procedural {

  /// **************************************************************************
  /// This class provides a more convenient interface to a Procedural channel
  /// than using the Cinema 4D parameter system. Note that we would really
  /// like to make all methods \c const, but the Cinema 4D API won't allow
  /// us to do so.
  ///
  /// Whenever a channel is changed and updates on other things that depend
  /// on it should be rolled out, the channel's \ref DIRTYFLAGS_DATA count
  /// should be increased using \c channel->SetDirty(DIRTYFLAGS_DATA) .
  /// **************************************************************************
  class Channel : public BaseTag
  {
    typedef BaseTag SUPER;
    Channel();
    ~Channel();
  public:

    /// @return the dirty count of the channel.
    inline Int32 GetChannelDirty()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_DIRTYCOUNT, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// @return the state of the Procedural channel.
    inline Int32 GetChannelState()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_STATE, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// @return the mode of the channel.
    inline Int32 GetChannelMode() {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_MODE, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// Sets the mode of the channel.
    inline Int32 SetChannelMode(Int32 mode)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(mode);
      return this->SetParameter(
        PROCEDURAL_CHANNEL_PARAM_MODE, data, DESCFLAGS_SET_0);
    }

    /// @return the \enum PROCEDURAL_CHANNEL_TYPE of the channel.
    inline Int32 GetChannelType()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_TYPE, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// Checks the Channel datatype and item length and returns \c true if it
    /// it matches with the specified values, \c false if not.
    /// @param[in] type The data type that the channel is expected to have.
    /// @param[in] itemLength The number of sub items that the channel is
    ///     is expected to have.
    /// @return \c true if the channel meets the expectations, \c false if not.
    inline Bool Check(Int32 type, Int32 itemLength) {
      if (this->GetChannelType() != type) return false;
      if (this->GetItemLength() != itemLength) return false;
      return true;
    }

    /// Sets the type of the channel.
    /// @param[in] type The new data type of the channel.
    /// @return \c true if the change of type was queued, \c false if not.
    inline Bool SetChannelType(Int32 type)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(type);
      return this->SetParameter(
        PROCEDURAL_CHANNEL_PARAM_TYPE, data, DESCFLAGS_SET_0);
    }

    /// @return the number of elements in the Procedural channel.
    inline Int32 GetCount()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_COUNT, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// Sets the element count of the channel.
    /// @param[in] count The new element count.
    /// @return \c true if the change of the element count was queued,
    ///     \c false if not.
    inline Bool SetCount(Int32 count)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(count);
      return this->SetParameter(
        PROCEDURAL_CHANNEL_PARAM_COUNT, data, DESCFLAGS_SET_0);
    }

    /// @return the number of items per element in the channel.
    inline Int32 GetItemLength()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(
        PROCEDURAL_CHANNEL_PARAM_ITEMLENGTH, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// Sets the number of items per element in the channel.
    inline Bool SetItemLength(Int32 count)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(count);
      return this->SetParameter(
        PROCEDURAL_CHANNEL_PARAM_ITEMLENGTH, data, DESCFLAGS_SET_0);
    }

    /// @return the number of frames allocated in the channel.
    inline Int32 GetFrameCount()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_FRAMECOUNT, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// Sets the number of frames to be allocated in the channel.
    inline Bool SetFrameCount(Int32 frameCount)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(frameCount);
      return this->SetParameter(PROCEDURAL_CHANNEL_PARAM_FRAMECOUNT, data, DESCFLAGS_SET_0);
    }

    /// @return the frame number that is currently used in the channel.
    inline Int32 GetFrame()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_FRAME, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// Sets the frame number that is currently used in the channel.
    inline Bool SetFrame(Int32 frame)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(frame);
      return this->SetParameter(PROCEDURAL_CHANNEL_PARAM_FRAME, data, DESCFLAGS_SET_0);
    }

    /// @return wether the frame is synchronised with the document frame.
    inline Bool GetSyncFrame()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_SYNCFRAME, data, DESCFLAGS_GET_0);
      return data.GetBool();
    }

    /// Sets wether the frame is synchronised with the document frame.
    inline Bool SetSyncFrame(Bool syncFrame)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(syncFrame);
      return this->SetParameter(PROCEDURAL_CHANNEL_PARAM_SYNCFRAME, data, DESCFLAGS_SET_0);
    }

    /// @return the frame offset added to the synchronized frame number.
    inline Int32 GetFrameOffset()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_FRAMEOFFSET, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// Sets wether the frame is synchronised with the document frame.
    inline Bool SetFrameOffset(Bool frameOffset)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(frameOffset);
      return this->SetParameter(PROCEDURAL_CHANNEL_PARAM_FRAMEOFFSET, data, DESCFLAGS_SET_0);
    }

    /// @return the index of the first element in the array taking
    ///   the current frame and frame offset into account.
    inline Int32 GetFrameOffsetIndex()
    {
      Int32 const frameCount = this->GetFrameCount();
      Int32 frame = this->GetFrame() % frameCount;
      if (frame < 0)
        frame += frameCount;
      return frame * this->GetCount() * this->GetItemLength();
    }

    /// Retreives all configured display colors of the channel and
    /// optionally the display flag. The arrays passed here must be
    /// of size #PROCEDURAL_CHANNEL_MAXITEMLENGTH .
    inline Bool GetDisplayColors(Bool* enabled, Vector* colors)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      BaseContainer const* bc = this->GetDataInstance();
      if (!bc) return false;
      for (Int32 index = 0; index < PROCEDURAL_CHANNEL_MAXITEMLENGTH; ++index) {
        Int32 const id = PROCEDURAL_CHANNEL_DISPLAY_COLOR_START +
          index * PROCEDURAL_CHANNEL_DISPLAY_COLOR_SLOTS;
        if (enabled) enabled[index] = bc->GetBool(id + 0);
        if (colors) colors[index] = bc->GetVector(id + 1);
      }
      return true;
    }

    /// @return the display mode.
    inline Int32 GetDisplayMode()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_DISPLAY_MODE, data, DESCFLAGS_GET_0);
      return data.GetInt32();
    }

    /// @return the display mode.
    inline Int32 SetDisplayMode(Int32 mode)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(mode);
      return this->SetParameter(
        PROCEDURAL_CHANNEL_DISPLAY_MODE, data, DESCFLAGS_SET_0);
    }

    /// @return the name of the reference channel for this channel.
    inline String GetRefChannelName()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_REF, data, DESCFLAGS_GET_0);
      return data.GetString();
    }

    /// Sets the name of the reference channel for this channel. Pass an empty
    /// string to make the channel not use a reference channel at all.
    /// @param[in] name The name of the reference channel to use.
    /// @return \c true if the name was set, \c false if not.
    inline Bool SetRefChannelName(const String& name)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(name);
      return this->SetParameter(
        PROCEDURAL_CHANNEL_PARAM_REF, data, DESCFLAGS_SET_0);
    }

    /// @return the lock state of the channel.
    inline Bool GetLocked()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_LOCKED, data, DESCFLAGS_GET_0);
      return data.GetBool();
    }

    /// Sets the lock state of the channel. Only a permanent channel can
    /// be locked. A locked channel can not be written until its unlocked.
    inline Bool SetLocked(Int32 locked)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data(locked);
      return this->SetParameter(
        PROCEDURAL_CHANNEL_PARAM_LOCKED, data, DESCFLAGS_SET_0);
    }

    /// @return The data handle of the channel. The handle is the memory
    ///     address of the contiguous memory block that contains the
    ///     elements of the channel. The handle can invalidate after changing
    ///     the type and/or size of the channel.
    /// <b>Note</b>: The data handle is built of \a n elements of the
    /// datatype selected in the channel where \a n is the number of elements
    /// times the array length per item.
    inline void* GetDataHandle()
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      GeData data;
      this->GetParameter(PROCEDURAL_CHANNEL_PARAM_HANDLE, data, DESCFLAGS_GET_0);
      return data.GetVoid();
    }

    /// @param[out] data The GeData to fill with the value.
    /// @param[in] index The index of the element to access.
    /// @param[in] subindex The subindex of the item to access.
    /// @return \c true if the element could be read, \c false if not (eg.
    ///     if \p index < 0 or \p index >= \c GetElementCount().
    inline Bool GetElement(GeData& data, Int32 index, Int32 subindex=0)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      DescID descid(PROCEDURAL_CHANNEL_PARAM_DATA, index + 1, subindex + 1);
      if (!this->GetParameter(descid, data, DESCFLAGS_GET_0)) {
        data = GeData();
        return false;
      }
      return true;
    }

    /// Sets the value of an element in the channel.
    /// @param[in] data The data of the element to set.
    /// @param[in] index The index of the element to write.
    /// @param[in] subindex The subindex of the item to write.
    /// @return \c true if the element could be written, \c false if not (eg.
    ///     if \p index < 0 or \p index >= \c GetElementCount() or if the
    ///     datatype of \p data can not be translated to the datatype of the
    ///     channel).
    inline Bool SetElement(const GeData& data, Int32 index, Int32 subindex)
    {
      DebugAssert(this->GetType() == PROCEDURAL_CHANNEL_ID);
      DescID descid(PROCEDURAL_CHANNEL_PARAM_DATA, index + 1, subindex + 1);
      return this->SetParameter(descid, data, DESCFLAGS_SET_FORCESET);
    }

    /// Reinitializes the channels items with the default value. Has no
    /// effect on a locked channel.
    inline void Reinitialize()
    {
      this->Message(MSG_PROCEDURAL_CHANNEL_REINIT);
    }

    /// Broadcasts a message to all neighbouring channels.
    inline void BroadcastMessage(Int32 msg, void* pData = nullptr)
    {
      BaseTag* pred = this->GetPred();
      while (pred) {
        if (pred->GetType() == PROCEDURAL_CHANNEL_ID)
          pred->Message(msg, pData);
        pred = pred->GetPred();
      }

      BaseTag* next = this->GetNext();
      while (next) {
        if (next->GetType() == PROCEDURAL_CHANNEL_ID)
          next->Message(msg, pData);
        next = next->GetNext();
      }
    }

    /// \addtogroup BaseList2D Overloads
    /// @{
    //Channel* GetNext() { return static_cast<Channel*>(SUPER::GetNext()); }
    //Channel* GetPred() { return static_cast<Channel*>(SUPER::GetPred()); }
    //Channel* GetDown() { return static_cast<Channel*>(SUPER::GetDown()); }
    //Channel* GetUp() { return static_cast<Channel*>(SUPER::GetUp()); }
    // @}


    /// Finds a Channel with the specified name in the same channel system as
    /// the channel this method is called with. Returns \c nullptr if not
    /// matching Channel was found or if the called tag itself was found.
    ///
    /// @param[in] name The name of the channel to find.
    /// @return The channel with the specified name or \c nullptr.
    inline Channel* Find(const String& name, Int32 type=NOTOK, Int32 itemLength=NOTOK)
    {
      return Channel::Find(this, name, type, itemLength);
    }

    /// Finds a Channel by name starting from the object *op*.
    static inline Channel* Find(
      BaseList2D* ref, const String& name,
      Int32 type=NOTOK, Int32 itemLength=NOTOK)
    {
      if (!ref) return nullptr;
      Bool allowSelf = true;
      BaseTag* tag = nullptr;
      if (ref->IsInstanceOf(Obase)) {
        tag = static_cast<BaseObject*>(ref)->GetFirstTag();
      }
      else if (ref->IsInstanceOf(Tbase)) {
        tag = static_cast<BaseTag*>(ref);
      }
      if (!tag) return nullptr;

      auto check = [name, type, itemLength](BaseTag* const tag) -> bool {
        if (tag->GetType() != PROCEDURAL_CHANNEL_ID) return false;
        if (tag->GetName() != name) return false;
        Channel* const ch = static_cast<Channel*>(tag);
        if (type != NOTOK && ch->GetChannelType() != type) return false;
        if (itemLength != NOTOK && ch->GetItemLength() != itemLength) return false;
        return true;
      };

      BaseTag* curr = tag->GetPred();
      while (curr) {
        if (check(curr))
          return static_cast<Channel*>(curr);
        curr = curr->GetPred();
      }

      curr = tag;
      if (!allowSelf) curr = curr->GetNext();
      while (curr) {
        if (check(curr))
          return static_cast<Channel*>(curr);
        curr = curr->GetNext();
      }

      return nullptr;
    }

  };

} // namespace procedural
} // namespace nr

#endif // PROCEDURAL_CHANNEL_H_
