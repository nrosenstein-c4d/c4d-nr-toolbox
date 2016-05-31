/* Copyright (C) 2013-2016  Niklas Rosenstein
 * All rights reserved. */

#include <lib_description.h>
#include "timehide.h"
#include "res/description/Hnrtoolbox.h"

struct THStats;

static void THClearDocument(BaseDocument* doc);

static void THProcessDocument(BaseDocument* doc, THStats& stats, Bool first_call=false);

static THStats GetStats(BaseSceneHook* hook, BaseDocument* doc);

enum TRACKSMODE
{
  TRACKSMODE_0,
  TRACKSMODE_ANIMATED,
  TRACKSMODE_PREVIEWRANGE,
};

enum UPDATETYPE
{
  UPDATETYPE_INIT,
  UPDATETYPE_NORMAL,
  UPDATETYPE_RESET,
};

struct THStats
{

  unsigned only_active   : 1;
  unsigned only_animated : 1;
  unsigned force_update  : 1;
  TRACKSMODE tracks_mode;
  BaseTime preview_min;
  BaseTime preview_max;

  const THStats* prev;

  /**
    * Creates a new THStats object with default (disabled) values.
    */
  THStats()
  : only_active(false), only_animated(false), force_update(false),
    tracks_mode(TRACKSMODE_0), preview_min(), preview_max(), prev(NULL) {
  }

  /**
    * Returns true if the THStats changed compared to its previous
    * state, false if not. Should only be called if the the THStats
    * have been initialized already or the :attr:`prev` field is
    * set (an assertion is triggered otherwise).
    */
  Bool AskUpdate() const
  {
    if (force_update) {
      // Now now, this force_update might not ALWAYS be necessary,
      // even if the caller thinks it is. If all options are disabled
      // so that no timehide processing is necessary and we have already
      // processed the document with that settings, nothing needs to
      // be done!
      if (!only_active && !only_animated && tracks_mode == TRACKSMODE_0
        && prev && (*this) == (*prev)) {
      return false;
      }
      return true;
    }
    if (prev == NULL) {
      GePrint("[TimeHide ERROR]: THStats.prev is NULL in AskUpdate()");
      return false;
    }
    return (*this) != (*prev);
  }

  /**
    * Initializes the THStats. If no :attr:`prev` is specified, it will
    * be filled with a pointer to static default values. The documents
    * minimum and maximum time will be set by this function.
    */
  void Init(BaseDocument* doc)
  {
    static THStats default_stats;
    if (prev == NULL) {
      prev = &default_stats;
    }
    BaseContainer bc = doc->GetData(DOCUMENTSETTINGS_GENERAL);
    preview_min = bc.GetTime(DOCUMENT_LOOPMINTIME);
    preview_max = bc.GetTime(DOCUMENT_LOOPMAXTIME);
  }

  /**
    * Copies the data from another THStats object, the :attr:`prev`
    * attribute is set to NULL intentionally.
    */
  void CopyFrom(const THStats& other)
  {
    *this = other;
    prev = NULL;
  }

  Bool operator == (const THStats& other) const
  {
    if (other.only_active   != only_active)   return false;
    if (other.only_animated != only_animated) return false;
    if (other.tracks_mode   != tracks_mode)   return false;
    if (other.preview_min   != preview_min)   return false;
    if (other.preview_max   != preview_max)   return false;
    return true;
  }

  Bool operator != (const THStats& other) const
  {
    return ! (*this == other);
  }

};

#define ID_TEMP_FOLDBITS        1030815
#define _ID_TAG_MOTIONSYSTEM    465003000

enum BRANCHITER
{
  BRANCHITER_STOP,
  BRANCHITER_CONTINUE,
  BRANCHITER_CONTINUE_ELSEWHERE,
};

struct BranchIterator {
  Bool RunIteration(GeListNode* root);
  virtual Bool Initialize(GeListNode* root) { return TRUE; }
  virtual BRANCHITER ProcessNode(GeListNode* current) = 0;
  virtual void Finalize() { }
};

static BRANCHITER PerformIteration(BranchIterator* iter, GeListNode* node) {
  BRANCHITER mode = iter->ProcessNode(node);
  switch (mode) {
    case BRANCHITER_CONTINUE:
      break; // Go deeper
    case BRANCHITER_STOP:
    case BRANCHITER_CONTINUE_ELSEWHERE:
    default:
      return mode; // Stop completely
  }

  GeListNode* child = node->GetDown();
  for (; child; child=child->GetNext()) {
    if (PerformIteration(iter, child) == BRANCHITER_STOP) {
      return BRANCHITER_STOP;
    }
  }

  BranchInfo branches[20];
  Int32 count = node->GetBranchInfo(branches, 20, GETBRANCHINFO_0);
  for (Int32 i=0; i < count; i++) {
    BranchInfo& branch = branches[i];
    GeListHead* head = branch.head;
    if (!head) {
      continue;
    }

    if (head->IsInstanceOf(ID_LISTHEAD)) {
      GeListNode* curr = head->GetFirst();
      for(; curr; curr=curr->GetNext()) {
        if (PerformIteration(iter, curr) == BRANCHITER_STOP) {
          return BRANCHITER_STOP;
        }
      }
    }
    else if (PerformIteration(iter, head) == BRANCHITER_STOP) {
      return BRANCHITER_STOP;
    }
  }

  return BRANCHITER_CONTINUE;
}

Bool BranchIterator::RunIteration(GeListNode* root) {
  if (!root) {
    return FALSE;
  }
  if (!Initialize(root)) {
    return FALSE;
  }
  PerformIteration(this, root);
  Finalize();
  return TRUE;
}

class THUpdater : public BranchIterator
{

  typedef BranchIterator super;

public:

  THUpdater(const THStats& stats=THStats(), UPDATETYPE type=UPDATETYPE_NORMAL)
  : super(), m_stats(stats), m_type(type) { }

  //| BranchIterator Overrides

  virtual BRANCHITER ProcessNode(GeListNode* node)
  {
    if (node->IsInstanceOf(CTbase) || node->IsInstanceOf(Xbase)) {
      // Tracks should of course be visible in the Timeline,
      // and for shaders, they can not be really "selected" in
      // the UI.
    }
    else if (node->IsInstanceOf(Tbaselist2d)) {
      SetStates(node, node, m_type);

      // Skip the Motion System completely.
      if (node->IsInstanceOf(_ID_TAG_MOTIONSYSTEM)) return BRANCHITER_CONTINUE_ELSEWHERE;
    }

    return BRANCHITER_CONTINUE;
  }

private:

  void SetStates(GeListNode* dest, GeListNode* reference, UPDATETYPE type)
  {
    SetStates((BaseList2D*) dest, (BaseList2D*) reference, type);
  }

  void SetStates(BaseList2D* dest, BaseList2D* reference, UPDATETYPE type)
  {
    Bool active = reference->GetBit(BIT_ACTIVE);
    Bool reset = type == UPDATETYPE_RESET;
    CTrack* track = dest->GetFirstCTrack();

    // Compute whether the element (which is anything but a CTrack) is visible
    // in the timeline.
    Bool visible = true;

    // Process the elements' tracks if the object is still visible
    // and we don't want to reset everything.
    if (!reset) {
      // Update the visibility of the Tracks. If no tracks are visible,
      // the object may be hidden as well?
      visible = false;
      for (CTrack* curr=track; curr; curr=curr->GetNext()) {
        if (SetTrackState(curr)) { // If the track is visible
          visible = true;
        }
      }

      // Again, we need to treat the Motion System tag specially. It
      // will (usually) never have any tracks, but Motion Layers. For
      // now, we will completely ignore the "Only Show Animated Elements"
      // option for the Motion System tag and display it always.
      // Note: The "Only Show Selected Elements" option still comes
      // into effect (earlier in the code)
      if (!visible) {
        if (dest->IsInstanceOf(_ID_TAG_MOTIONSYSTEM))
          visible = true;
        else
          visible = !m_stats.only_animated;
      }
    }

    // If "Only Show Selected Elements" is on, it should override
    // the "Only Show Animated Elements" option.
    if (!reset && (m_stats.only_active || m_stats.prev->only_active != m_stats.only_active)) {
      visible = !m_stats.only_active || active;
    }

    // And, well, if the object is not visible, the tracks should not be
    // visible either.
    if (!reset && !visible) {
      for (CTrack* curr=track; curr; curr=curr->GetNext()) {
        SetNBitRange(curr, NBIT_TL1_HIDE, 4, true);
      }
    }

    // New in 1.3: Hide the shaders of a node if it is not visible
    // in the Timeline either.
    for (BaseShader* curr=dest->GetFirstShader(); curr; curr=curr->GetNext()) {
      UpdateShaderTree(curr, (!reset && !visible));
    }

    // To show sub elements when they are the only thing selected
    // in a document, their parent elements must have been unfolded
    // in the timeline. We store the previous states of the bits.
    BaseContainer* bc = dest->GetDataInstance();
    GeData* dataPtr = NULL;
    if (bc) {
      bc->FindIndex(ID_TEMP_FOLDBITS, &dataPtr);
    }
    if (!visible && (type == UPDATETYPE_INIT || !dataPtr)) {
      if (!dataPtr) {
        bc->SetInt32(ID_TEMP_FOLDBITS, SumNBits(dest, NBIT_TL1_FOLDTR, 4));
      }
      UnpackNBits(dest, NBIT_TL1_FOLDTR, 4, 0xffffffff);
    }
    else if ((reset || visible) && dataPtr) {
      UnpackNBits(dest, NBIT_TL1_FOLDTR, 4, dataPtr->GetInt32());
      bc->RemoveData(ID_TEMP_FOLDBITS);
    }

    SetNBitRange(dest, NBIT_TL1_HIDE, 4, !visible);
  }

  Bool SetTrackState(CTrack* track)
  {
    Bool visible = true;
    if (visible && (m_stats.tracks_mode != TRACKSMODE_0 || m_stats.prev->tracks_mode != m_stats.tracks_mode)) {
      const CCurve* curve = track->GetCurve();
      const Int32 key_count = (curve ? curve->GetKeyCount() : 0);
      const CKey* first_key = (key_count > 0 ? curve->GetKey(0) : NULL);
      const CKey* last_key = (key_count > 0 ? curve->GetKey(key_count - 1) : NULL);

      // Is the "unanimated" parameter set, or was it turned off? If so,
      // we need to update the tracks.
      switch (m_stats.tracks_mode) {
        case TRACKSMODE_ANIMATED:
          if (!first_key) visible = false;
          break;
        case TRACKSMODE_PREVIEWRANGE:
          if (first_key && last_key) {
            if (first_key->GetTime() > m_stats.preview_max) visible = false;
            else if (last_key->GetTime() < m_stats.preview_min) visible = false;
          }
          else visible = false;
          break;
        case TRACKSMODE_0:
        default:
          break;
      }
    }
    SetNBitRange(track, NBIT_TL1_HIDE, 4, !visible);
    return visible;
  }

  /// Hides the complete hierarchy of `node` depending on the
  /// visibility of the owning Material (or node) and the options.
  /// Returns True if the were any tracks left visible in the tree.
  Bool UpdateShaderTree(BaseShader* shader, Bool mat_hidden)
  {

    // True if any track of this shader or sub-shader was
    // left visible in the tree.
    Bool any_visible = false;

    // Start with propagating to the child-nodes.
    BaseShader* child = shader->GetDown();
    for (; child; child=child->GetNext()) {
      if (UpdateShaderTree(child, mat_hidden))
        any_visible = true;
    }

    // Hide/Show the tracks of the node depending on the
    // options, but only if mat_hidden is false.
    CTrack* track = shader->GetFirstCTrack();
    for (; track; track=track->GetNext()) {
      if (mat_hidden)
        SetNBitRange(track, NBIT_TL1_HIDE, 4, true);
      else
        if (SetTrackState(track))
          any_visible = true;
    }

    // Only display this node if any of its tracks or
    // tracks of its child nodes are visible, AND if the
    // the owning material is visible.
    Bool hide_node = mat_hidden;
    if (!mat_hidden && m_stats.only_animated) {
      hide_node = !any_visible;
    }
    SetNBitRange(shader, NBIT_TL1_HIDE, 4, hide_node);

    return !hide_node;
  }


  void SetNBitRange(BaseList2D* dest, NBIT start, Int32 count, Bool mode)
  {
    NBITCONTROL control = mode ? NBITCONTROL_SET : NBITCONTROL_CLEAR;
    for (Int32 i=0; i < count; i++) {
      NBIT bit = (NBIT) ((Int32) start + i);
      dest->ChangeNBit(bit, control);
    }
  }

  Int32 SumNBits(BaseList2D* dest, NBIT start, Int32 count)
  {
    Int32 value = 0;
    for (Int32 i=0; i < count; i++) {
      Int32 j = sizeof(value) - i - 1;
      NBIT bit = (NBIT) ((Int32) start + i);
      if (dest->GetNBit(bit)) {
        value |= 1 << j;
      }
    }
    return value;
  }

  void UnpackNBits(BaseList2D* dest, NBIT start, Int32 count, Int32 value)
  {
    for (Int32 i=0; i < count; i++) {
      Int32 j = sizeof(value) - i - 1;
      NBIT bit = (NBIT) ((Int32) start + i);
      if (value & (1 << j)) {
        dest->ChangeNBit(bit, NBITCONTROL_SET);
      }
      else {
        dest->ChangeNBit(bit, NBITCONTROL_CLEAR);
      }
    }
  }

  THStats m_stats;
  UPDATETYPE m_type;

};

void THClearDocument(BaseDocument* doc)
{
  THStats stats;
  stats.force_update = true;
  stats.Init(doc);

  THUpdater updater(stats, UPDATETYPE_RESET);
  updater.RunIteration(doc);
}

void THProcessDocument(BaseDocument* doc, THStats& stats, Bool first_call)
{
  if (doc == NULL) {
    GePrint("[TimeHide ERROR]: THProcessDocument() without document.");
    return;
  }

  stats.Init(doc);
  if (first_call) stats.force_update = true;
  if (!stats.AskUpdate()) {
    return;
  }

  UPDATETYPE type = first_call ? UPDATETYPE_INIT : UPDATETYPE_NORMAL;
  THUpdater updater(stats, type);
  updater.RunIteration(doc);
}

THStats GetStats(BaseSceneHook* hook, BaseDocument* doc, THStats* m_stats)
{
  THStats stats;
  BaseContainer* bc = hook->GetDataInstance();
  if (!bc) return stats;

  stats.only_active = bc->GetBool(NRTOOLBOX_HOOK_TIMEHIDE_ONLYSELECTED);
  stats.only_animated = bc->GetBool(NRTOOLBOX_HOOK_TIMEHIDE_ONLYANIMATED);
  switch (bc->GetInt32(NRTOOLBOX_HOOK_TIMEHIDE_TRACKS)) {
    case NRTOOLBOX_HOOK_TIMEHIDE_TRACKS_ANIMATED:
      stats.tracks_mode = TRACKSMODE_ANIMATED;
      break;
    case NRTOOLBOX_HOOK_TIMEHIDE_TRACKS_PREVIEWRANGE:
      stats.tracks_mode = TRACKSMODE_PREVIEWRANGE;
      break;
    case NRTOOLBOX_HOOK_TIMEHIDE_TRACKS_SHOWALL:
    default:
      stats.tracks_mode = TRACKSMODE_0;
      break;
  }

  stats.prev = m_stats;
  stats.Init(doc);
  return stats;
}

THStats* THMessage(THStats* m_stats, BaseSceneHook* hook, BaseDocument* doc, Int32 msg, void* pdata)
{
  THStats stats;
  Bool updated = false;

  switch (msg) {
    case MSG_TIMEHIDE_EVMSG_CHANGE:
    case MSG_DESCRIPTION_POSTSETPARAMETER: {
      stats = GetStats(hook, doc, m_stats);
      updated = true;

      if (msg == MSG_TIMEHIDE_EVMSG_CHANGE) {
        // THProcessDocument() only actually processes the document
        // if the parameters changed OR if force_update is true.
        // Since the parameters have not changed when only an
        // EVMSG_CHANGE occured, we need to force the update.
        stats.force_update = true;
      }
      THProcessDocument(doc, stats);

      if (msg != MSG_TIMEHIDE_EVMSG_CHANGE) {
        EventAdd();
      }
      break;
    }
    case MSG_DOCUMENTINFO: {
      DocumentInfoData* data = (DocumentInfoData*) pdata;
      if (!hook || !data) {
        break;
      }

      stats = GetStats(hook, doc, m_stats);
      updated = true;
      switch (data->type) {
        case MSG_DOCUMENTINFO_TYPE_SAVE_BEFORE:
          // Unhide all elements before saving.
          THClearDocument(doc);
          break;
        case MSG_DOCUMENTINFO_TYPE_SAVE_AFTER:
        default:
          // Process them respectively in any other case.
          Bool first_call = data->type == MSG_DOCUMENTINFO_TYPE_SAVE_AFTER;
          THProcessDocument(doc, stats, first_call);
          break;
      }
      break;
    }
  }

  if (!m_stats)
    m_stats = NewObjClear(THStats);

  // Update the last stats so they can be compared to check if
  // the next update is actually required.
  if (updated && m_stats) m_stats->CopyFrom(stats);
  return m_stats;
}

void THFreeStats(THStats*& stats)
{
  DeleteMem(stats);
}
