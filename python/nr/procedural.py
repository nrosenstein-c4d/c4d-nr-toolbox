# Copyright (C) 2015 Niklas Rosenstein
# All rights reserved.

from __future__ import print_function, absolute_import

__author__ = 'Niklas Rosenstein <rosensteinniklas(at)gmail.com>'
__version__ = '0.1-dev'

import c4d


class Channel(object):
  ''' Wrapper for a Procedural Channel Tag. '''

  def __init__(self, op):
    super(Channel, self).__init__()
    assert op.GetType() == c4d.PROCEDURAL_CHANNEL_ID
    self.op = op

  def __repr__(self):
    type_name = {
      c4d.PROCEDURAL_CHANNEL_TYPE_NIL: 'Nil',
      c4d.PROCEDURAL_CHANNEL_TYPE_INTEGER: 'Integer',
      c4d.PROCEDURAL_CHANNEL_TYPE_FLOAT: 'Float',
      c4d.PROCEDURAL_CHANNEL_TYPE_VECTOR: 'Vector',
      c4d.PROCEDURAL_CHANNEL_TYPE_MATRIX: 'Matrix',
      c4d.PROCEDURAL_CHANNEL_TYPE_STRING: 'String',
    }.get(self.GetChannelType(), '???')
    fmt = '<{0} Channel {1!r} count={2} item_length={3} frame_count={4} frame={5}>'
    return fmt.format(type_name, self.GetName(), self.GetCount(),
      self.GetItemLength(), self.GetFrameCount(), self.GetFrame())

  def GetName(self):
    return self.op.GetName()

  def SetName(self, name):
    self.op.SetName(name)

  def Message(self, mid, data=None):
    return self.op.Message(mid, data)

  def GetChannelDirty(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_DIRTYCOUNT]

  def GetChannelState(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_STATE]

  def GetChannelMode(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_MODE]

  def SetChannelMode(self, mode):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_MODE] = mode

  def GetChannelType(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_TYPE]

  def SetChannelType(self, type_):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_TYPE] = type_

  def Check(self, type_, item_length=None):
    if self.GetChannelType() != type_:
      return False
    if item_length is not None and self.GetItemLength() != item_length:
      return False
    return True

  def GetCount(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_COUNT]

  def SetCount(self, count):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_COUNT] = count

  def GetItemLength(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_ITEMLENGTH]

  def SetItemLength(self, length):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_ITEMLENGTH] = length

  def GetFrameCount(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_FRAMECOUNT]

  def SetFrameCount(self, count):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_FRAMECOUNT] = count

  def GetFrame(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_FRAME]

  def SetFrame(self, frame):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_FRAME] = frame

  def GetSyncFrame(self):
    return self.op[PROCEDURAL_CHANNEL_PARAM_SYNCFRAME]

  def SetSyncFrame(self, sync_frame):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_SYNCFRAME] = frame

  def GetFrameOffset(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_FRAMEOFFSET]

  def SetFrameOffset(self, frame_offset):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_FRAMEOFFSET] = frame_offset

  def GetFrameOffsetIndex(self):
    frame_count = self.GetFrameCount()
    frame = self.GetFrame() % frame_count
    if frame < 0:
      frame += frame_count
    return frame * self.GetCount() * self.GetItemLength()

  def GetRefChannelName(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_REF]

  def SetRefChannelName(self, name):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_REF] = name

  def GetLocked(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_LOCKED]

  def SetLocked(self, locked):
    self.op[c4d.PROCEDURAL_CHANNEL_PARAM_LOCKED] = bool(locked)

  def GetDataHandle(self):
    return self.op[c4d.PROCEDURAL_CHANNEL_PARAM_HANDLE]

  def _MkDescid(self, index, subindex):
    return c4d.DescID(
      c4d.DescLevel(c4d.PROCEDURAL_CHANNEL_PARAM_DATA),
      c4d.DescLevel(index + 1),
      c4d.DescLevel(subindex + 1))

  def GetElement(self, index, subindex=None):
    ''' GetElement(index, [subindex]) -> item or list of items

    Returns the element at the specified *index* which is always
    a list if *subindex* is not specified. If *subindex* is
    specified, returns only the item at the subindex.

    Raises:
      IndexError: If the element could not be read. '''

    length = self.GetItemLength()
    if subindex is None:
      items = [None] * length
      for i in xrange(length):
        descid = self._MkDescid(index, i)
        items[i] = self.op[descid]
        if items[i] is None:
          if i != 0:
            # Strange, we were able to access some other element
            # before this one.
            raise RuntimeError('strange behaviour, investiage')
          raise IndexError('index {0} out of bounds'.format(index))
      return items
    else:
      if subindex < 0 or subindex >= length:
        raise IndexError('subindex {0} out of bounds'.format(subindex))
      descid = self._MkDescid(index, subindex)
      result = self.op[descid]
      if result is None:
        raise IndexError('index {0} out of bounds'.format(index))
      return result

  def SetElement(self, index, subindex, value):
    ''' SetElement(index, subindex, value)

    Sets an element at the specified subindex. '''

    length = self.GetItemLength()
    if subindex < 0 or subindex >= length:
      raise IndexError('subindex {0} out of bounds'.format(index))
    descid = self._MkDescid(index, subindex)
    self.op[descid]= value

  def Reinitialize(self):
    self.op.Message(c4d.MSG_PROCEDURAL_CHANNEL_REINIT)

  @staticmethod
  def Find(ref, name, type_=None, item_length=None):
    ''' Finds the Channel Tag with the specified *name* in starting from
    the reference channel *ref*. Simply pass the first tag of an object
    if you have no channel to start searching from.

    Args:
      ref: A `c4d.BaseObject`, `c4d.BaseTag` or `Channel` instance.
        If a tag or channel is passed, the original channel will be
        ignored for the search.
    '''

    allow_self = False
    if isinstance(ref, Channel):
      ref = ref.op
    elif isinstance(ref, c4d.BaseObject):
      ref = ref.GetFirstTag()
      allow_self = True
    elif not isinstance(ref, c4d.BaseTag):
      raise TypeError('expected BaseObject, BaseTag or Channel', type(ref))

    def check(curr):
      if curr.GetType() != c4d.PROCEDURAL_CHANNEL_ID: return False
      if curr.GetName() != name: return False
      curr = Channel(curr)
      if type_ is not None and curr.GetChannelType() != type_: return False
      if item_length is not None and item_length != curr.GetItemLength():
        return False
      return True

    curr = ref.GetPred()
    while curr:
      if check(curr):
        return Channel(curr)
      curr = curr.GetPred()

    if allow_self:
      curr = ref
    else:
      curr = ref.GetNext()
    while curr:
      if check(curr):
        return Channel(curr)
      curr = curr.GetNext()

    return None
