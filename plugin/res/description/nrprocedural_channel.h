/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file res/description/nrprocedural_channel.h
/// \lastmodified 2015/05/29

#ifndef NRPROCEDURAL_CHANNEL_H_
#define NRPROCEDURAL_CHANNEL_H_

// Next ID: 4018

enum
{
  /// Plugin ID for the Procedural channel NodeData plugin.
  PROCEDURAL_CHANNEL_ID = 1035268,

  /// The maximum value for \var PROCEDURAL_CHANNEL_PARAM_ITEMELENGTH.
  PROCEDURAL_CHANNEL_MAXITEMLENGTH = 16,

  /// Main group for the channel parameters.
  PROCEDURAL_CHANNEL_PROPERTIES = 4014,

  /// This parameter contain the dirty count of the channel. This dirty
  /// count is kept separate from the Cinema 4D standard datatypes to
  /// make sure nothing can interfere with it. It is increased automatically
  /// when the channel's datatype, count or item length is changed. It
  /// should generally be changed (though this is not done automatically)
  /// when the data in the channel was modified by sending MSG_CHANGE to
  /// the channel.
  PROCEDURAL_CHANNEL_DIRTYCOUNT = 4017,

  /// \addtogroup PROCEDURAL_CHANNEL_TYPE
  /// @{
  PROCEDURAL_CHANNEL_TYPE_NIL = 0,
  PROCEDURAL_CHANNEL_TYPE_INTEGER,
  PROCEDURAL_CHANNEL_TYPE_FLOAT,
  PROCEDURAL_CHANNEL_TYPE_VECTOR,
  PROCEDURAL_CHANNEL_TYPE_MATRIX,
  PROCEDURAL_CHANNEL_TYPE_STRING,
  /// @}

  /// \addtogroup PROCEDURAL_CHANNEL_STATE
  /// @{
  PROCEDURAL_CHANNEL_STATE_UNINITIALIZED = 0,
  PROCEDURAL_CHANNEL_STATE_INITIALIZED,
  PROCEDURAL_CHANNEL_STATE_TYPEERROR,
  PROCEDURAL_CHANNEL_STATE_MEMORYERROR,
  PROCEDURAL_CHANNEL_STATE_REFERROR,
  PROCEDURAL_CHANNEL_STATE_UNKNOWN,
  /// @}

  /// \addtogroup PROCEDURAL_CHANNEL_MODE
  /// @{
  PROCEDURAL_CHANNEL_MODE_FLOATING = 0,
  PROCEDURAL_CHANNEL_MODE_PERMANENT,
  /// @}

  /// \addtogroup PROCEDURAL_CHANNEL_PARAM
  /// Parameter IDs for a Procedural channel. The native Cinema 4D parameter
  /// system can be used to interact with a channel.
  /// @{

  /// If this parameter is \c true, the channel is locked and does not
  /// permit any write actions. Only a permanent channel can be locked.
  PROCEDURAL_CHANNEL_PARAM_LOCKED = 4007,

  /// The mode of the channel. There are three options available:
  /// - Floating: The channel is a simple data container but is not
  ///   saved to a file when saving the Cinema 4D scene. This is
  ///   the default behaviour.
  /// - Permanent: The channel is a simple data container that is
  ///   saved to a file when saving the Cinema 4D scene. The data
  ///   is therefore permanently saved with the Cinema 4D file.
  PROCEDURAL_CHANNEL_PARAM_MODE = 4006,

  /// This parameter ID delivers the datatype of the channl. Writing this
  /// parameter does not immediately commit the change, send \var MSG_UPDATE
  /// to do so.
  PROCEDURAL_CHANNEL_PARAM_TYPE = 4000,

  /// This parmater ID delivers the number of elements in the channel.
  /// Writing this parameter does not immediately commit the change, send
  /// \var MSG_UPDATE to do so.
  PROCEDURAL_CHANNEL_PARAM_COUNT = 4001,

  /// This parameter controls whether an array of the datatype is allocated
  /// for each datatype instead of just a single element.
  PROCEDURAL_CHANNEL_PARAM_ITEMLENGTH = 4008,

  /// Group ID for the frame settings.
  PROCEDURAL_CHANNEL_GROUP_FRAMESETTINGS = 4013,

    /// This parameter controls the number of frames to allocate. Note that
    /// incrasing this parameter will have large impact on the amount of
    /// memory allocated for the channel, treat with caution.
    PROCEDURAL_CHANNEL_PARAM_FRAMECOUNT = 4009,

    /// The current frame number of the channel.
    PROCEDURAL_CHANNEL_PARAM_FRAME = 4010,

    /// If this parameter is enabled, the Channel will synchronize the
    /// #PROCEDURAL_CHANNEL_PARAM_FRAME with the documents current frame
    /// number, taking the #PROCEDURAL_CHANNEL_PARAM_FRAMEOFFSET into account.
    PROCEDURAL_CHANNEL_PARAM_SYNCFRAME = 4011,

    /// The frame offset to consider when #PROCEDURAL_CHANNEL_PARAM_SYNCFRAME
    /// is enabled.
    PROCEDURAL_CHANNEL_PARAM_FRAMEOFFSET = 4012,

    /// This parameter delivers the state of the channel and returns one
    /// of the \enum PROCEDURAL_CHANNEL_STATE values. This parameter is
    /// read only. The UI for this parameter is a combobox but is always
    /// greyed out.
    PROCEDURAL_CHANNEL_PARAM_STATE = 4002,

  /// This paramater can contain the name of another channel inside the
  /// same channel system. If the string is non-empty, the current
  /// channel will inherit the size of the designated channel. It will
  /// recursively send \var MSG_UPDATE to the dependent channel. If the
  /// channel can not be found or if a cyclic reference happens, the
  /// channel will be in an error state.
  PROCEDURAL_CHANNEL_PARAM_REF = 4003,

  /// This parameter is read only and delivers the memory address of
  /// the internal memory that contains the data. The data is a contiguous
  /// memory location of the channels data elements.
  PROCEDURAL_CHANNEL_PARAM_HANDLE = 4004,

  /// Parameter ID to access the data in a channel. Must be used as the
  /// first ID in a DescID and the index of the element as the
  /// second ID. The indexing starts at one (as opposed to normally when
  /// indexing starts at zero) because a zero DescLevel does not
  /// exist. If a subindex should be accessed, a third index can be used.
  /// \code{.cpp}
  /// DescID index(PROCEDURAL_CHANNEL_PARAM_DATA, 42 + 1, 0 + 1);
  /// \endcode
  PROCEDURAL_CHANNEL_PARAM_DATA = 4005,

  /// Group for the display tab.
  PROCEDURAL_CHANNEL_GROUP_DISPLAY = 6000,  // GROUP

  PROCEDURAL_CHANNEL_DISPLAY_MODE = 6001,  // CYCLE
    PROCEDURAL_CHANNEL_DISPLAY_MODE_OFF = 0,
    PROCEDURAL_CHANNEL_DISPLAY_MODE_LOCAL = 1,
    PROCEDURAL_CHANNEL_DISPLAY_MODE_GLOBAL = 2,
    PROCEDURAL_CHANNEL_DISPLAY_MODE_REFTEXT = 3,
    PROCEDURAL_CHANNEL_DISPLAY_MODE_REFVIS = 4,

  PROCEDURAL_CHANNEL_GROUP_DISPLAY_COLORS = 6099,  // GROUP
    PROCEDURAL_CHANNEL_DISPLAY_COLOR_START = 6100,
    PROCEDURAL_CHANNEL_DISPLAY_COLOR_SLOTS = 5,
    PROCEDURAL_CHANNEL_DISPLAY_COLOR_SLOT_ENABLED = 0,  // BOOL
    PROCEDURAL_CHANNEL_DISPLAY_COLOR_SLOT_COLOR = 1,  // COLOR

  /// \addtogroup DefaultValues
  /// The default values to initialize every item in the channel with.
  /// @{
  PROCEDURAL_CHANNEL_PARAM_DEFAULTV_INTEGER = 5000,
  PROCEDURAL_CHANNEL_PARAM_DEFAULTV_FLOAT = 5001,
  PROCEDURAL_CHANNEL_PARAM_DEFAULTV_VECTOR = 5002,
  PROCEDURAL_CHANNEL_PARAM_DEFAULTV_MATRIX = 5003,
  PROCEDURAL_CHANNEL_PARAM_DEFAULTV_STRING = 5004,
  /// @}
  /// @}

  /// \addtogroup PROCEDURAL_CHANNEL_UI
  /// @{
  PROCEDURAL_CHANNEL_UI_TINTENABLED = 7004,
  PROCEDURAL_CHANNEL_UI_TINTCOLOR = 7005,
  PROCEDURAL_CHANNEL_UI_MANUALREINIT = 7003,
  PROCEDURAL_CHANNEL_UI_GOTOREF = 7001,
  PROCEDURAL_CHANNEL_UI_MEMINFO = 7002,
  /// @}
};

#endif // NRPROCEDURAL_CHANNEL_H_
