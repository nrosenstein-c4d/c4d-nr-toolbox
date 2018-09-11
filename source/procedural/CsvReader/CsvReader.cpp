/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file procedural/CsvReader/CsvReader.cpp
/// \lastmodified 2015/07/10

#include <c4d_apibridge.h>
#include "fs.hpp"
#include "CsvReader.h"
#include "DescriptionHelper.h"
#include "res/description/nrprocedural_csvreader.h"
#include "misc/print.h"

#include <nr/procedural/channel.h>
#include <NiklasRosenstein/csv.hpp>
#include <NiklasRosenstein/c4d/description.hpp>
#include <NiklasRosenstein/c4d/utils.hpp>
#include <NiklasRosenstein/c4d/raii.hpp>
#include <NiklasRosenstein/c4d/string.hpp>
#include <NiklasRosenstein/c4d/storage.hpp>
#include <NiklasRosenstein/math.hpp>

namespace nr { using namespace niklasrosenstein; }

using c4d_apibridge::IsEmpty;
using c4d_apibridge::GetDescriptionID;
using nr::c4d::tostr;
using nr::clamp;
using nr::procedural::Channel;

/// **************************************************************************
/// **************************************************************************
template <typename T, class C>
inline T get(C const& data, size_t index, T const& default_value)
{
  if (index < 0 || index >= data.size())
    return default_value;
  return data[index];
}

namespace std {
  std::string to_string(::String const& s) {
    return std::string(nr::c4d::auto_string(s));
  }
}
namespace nr {
  ::String tostr(std::string const& s) {
    return String(s.c_str());
  }
}

/*!
 * Helper class for working with CSV files that eventually need to be
 * reloaded at a later point in the program.
 */
class csv_table
{
public:
  using csv_row = nr::csv_row;

  /* Default constructor. Sets @delim to {','} and @quote to {'"'}. */
  csv_table() : info_() { this->clear(); }

  /* Destructor. */
  ~csv_table() { }

  /* Get the list of @csv_row%s. */
  std::vector<csv_row> const& rows() const { return this->rows_; }

  /* Get a @csv_row by index. */
  csv_row const& row(size_t index) const
  {
    assert(index >= 0 && index < this->rows_.size());
    return this->rows_[index];
  }

  /* Return the number of rows. */
  size_t row_count() const { return this->rows_.size(); }

  /* Return the length of the smallest row in the table. */
  size_t row_min() const { return this->rowmin_; }

  /* Return the length of the largest row in the table. */
  size_t row_max() const { return this->rowmax_; }

  /* Get/set the delimiter. */
  char& delim() { return this->info_.delim; }

  /* Get/set the quote. */
  char& quote() { return this->info_.quote; }

  /* Flush the table data. */
  void clear()
  {
    this->loaded_ = false;
    this->file_ = "";
    this->ftime_ = 0;
    this->rowmin_ = 0;
    this->rowmax_ = 0;
    this->rows_.clear();
  }

  /*!
   * Initialize the table with the specified @file name. If the name
   * or filetime didn't change, the table is not reloaded. If the file
   * no longer exists, the table is cleared.
   */
  bool init(std::string const& file, bool force_reload = false, bool* reloaded = nullptr)
  {
    if (reloaded) *reloaded = false;

    // Check if we need to reload the file.
    if (!force_reload && file == this->file_) {
      uint64_t ftime = fs::getmtime(file);
      if (ftime == this->ftime_) {
        return true;
      }
      this->ftime_ = ftime;
    }
    else {
      this->file_ = file;
      this->ftime_ = fs::getmtime(file);
    }

    // Flush and reload the CSV data.
    this->clear();
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) {
      return false;
    }
    auto callback = [this](csv_row const& row)
    {
      if (this->rowmin_ == 0 || row.size() < this->rowmin_)
        this->rowmin_ = row.size();
      if (this->rowmax_ == 0 || row.size() > this->rowmax_)
        this->rowmax_ = row.size();
      this->rows_.push_back(row);
      return true;
    };
    nr::csv_parse(fp, callback, this->info_);

    if (reloaded) *reloaded = true;
    this->loaded_ = true;
    return true;
  }

private:

  bool loaded_;
  std::string file_;
  uint64_t ftime_;
  size_t rowmin_, rowmax_;
  std::vector<csv_row> rows_;
  nr::csv_info info_;
};

/// **************************************************************************
/// Utility class used to convert values from a \ref csv_table
/// to other values. Use \ref `operator Bool` to check if the value could
/// be retrieved.
/// **************************************************************************
class CsvTableConverter
{
public:
  CsvTableConverter(const csv_table& table)
    : m_success(false), m_table(table) { }
  ~CsvTableConverter() { }
  inline operator Bool () const { return m_success; }
  template <typename T>
  inline T Convert(T(*F)(const String&, Bool* success), Int32 row, Int32 column)
  {
    m_success = false;
    if (row >= m_table.row_count()) {
      m_success = true;
    }
    else if (row >= 0) {
      csv_table::csv_row const& data = m_table.row(row);
      if (column >= 0 && column < data.size())
        return F(tostr(data[column]), &m_success);
    }
    return T();
  }
private:
  Bool m_success;
  const csv_table& m_table;
};

/// **************************************************************************
/// **************************************************************************
struct CsvEntry
{
public:
  CsvEntry(Int32 index)
  {
    mIndex = index;
    mBaseId = PROCEDURAL_CSVREADER_ENTRYMINID +
      index * PROCEDURAL_CSVREADER_ENTRYSIZE;
  }

  inline Int32 operator + (Int32 index) const { return mBaseId + index; }

  Bool Init(BaseContainer* bc) const;

  Bool FillDescription(
    BaseTag* host, const BaseContainer& cycle,
    Description* desc, const DescID& parentId) const;

  void Update(BaseTag* host, const csv_table& table) const;

  static inline Int32 Revert(Int32 itemId)
  {
    itemId -= PROCEDURAL_CSVREADER_ENTRYMINID;
    itemId /= PROCEDURAL_CSVREADER_ENTRYSIZE;
    if (itemId < 0 || itemId >= PROCEDURAL_CSVREADER_MAXENTRIES)
      return -1;
    return itemId;
  }

private:
  Int32 mIndex, mBaseId;
};

/// **************************************************************************
/// **************************************************************************
Bool CsvEntry::Init(BaseContainer* bc) const
{
  bc->SetString(mBaseId + PROCEDURAL_CSVREADER_ENTRY_CHANNELNAME, ""_s);
  bc->SetInt32(mBaseId + PROCEDURAL_CSVREADER_ENTRY_LASTDIRTYCOUNT, 0);
  for (Int32 index = 0; index < 12; ++index)
    bc->SetInt32(mBaseId + PROCEDURAL_CSVREADER_ENTRY_COLUMNSTART + index, 0);
  return true;
}

/// **************************************************************************
/// **************************************************************************
Bool CsvEntry::FillDescription(
  BaseTag* host, const BaseContainer& cycle,
  Description* desc, const DescID& parentId) const
{
  BaseContainer bc;
  BaseContainer* data = host->GetDataInstance();
  if (!data) return false;

  const Int32 channelNameId = mBaseId + PROCEDURAL_CSVREADER_ENTRY_CHANNELNAME;
  const String channelName = data->GetString(channelNameId);
  const String groupName = "#" + tostr(mIndex + 1) + " " + channelName;

  DescriptionHelper dh(desc);
  DescID cid;
  const DescID groupId = dh.MakeGroup(
    mBaseId + PROCEDURAL_CSVREADER_ENTRY_GROUP, parentId,
    groupName, "", 1, true);

  // Add the channel name parameter.
  cid = DescLevel(channelNameId, DTYPE_STRING, 0);
  if (dh.CheckSingleID(cid)) {
    bc = GetCustomDataTypeDefault(DTYPE_STRING);
    bc.SetString(DESC_NAME, "Target Channel"_s);
    bc.SetBool(DESC_SCALEH, true);
    bc.SetInt32(DESC_ANIMATE, DESC_ANIMATE_OFF);
    desc->SetParameter(cid, bc, groupId);
  }

  // Search for the channel and add parameters based on its type.
  Channel* const channel = Channel::Find(host, channelName);
  if (!channel) return true;

  // Helper function to add a new CSV Cycle parameter.
  bc = GetCustomDataTypeDefault(DTYPE_LONG);
  auto addEntry = [&bc, desc, groupId, cycle, &dh](Int32 id, const String& name, Int32 addIndex) {
    if (!dh.CheckSingleID(id)) return;
    String newName = name + String::IntToString(addIndex);
    BaseContainer idesc = bc;
    idesc.SetString(DESC_NAME, newName);
    idesc.SetContainer(DESC_CYCLE, cycle);
    idesc.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_CYCLE);
    idesc.SetInt32(DESC_ANIMATE, DESC_ANIMATE_OFF);
    DescID did = DescLevel(id, DTYPE_LONG, 0);
    desc->SetParameter(did, idesc, groupId);
  };

  const Int32 baseId = mBaseId + PROCEDURAL_CSVREADER_ENTRY_COLUMNSTART;
  const Int32 channelType = channel->GetChannelType();
  const Int32 itemLength = channel->GetItemLength();
  switch (channelType) { // switch: PROCEDURAL_CHANNEL_TYPE
    case PROCEDURAL_CHANNEL_TYPE_INTEGER:
      for (Int32 index = 0; index < itemLength; ++index)
        addEntry(baseId + index, "Integer . ", index);
      break;
    case PROCEDURAL_CHANNEL_TYPE_FLOAT:
      for (Int32 index = 0; index < itemLength; ++index)
        addEntry(baseId + index, "Float . ", index);
      break;
    case PROCEDURAL_CHANNEL_TYPE_STRING:
      for (Int32 index = 0; index < itemLength; ++index)
        addEntry(baseId + index, "String . ", index);
      break;
    case PROCEDURAL_CHANNEL_TYPE_VECTOR:
      for (Int32 index = 0; index < itemLength; ++index) {
        addEntry(baseId + index * 3 + 0, "Vector . X . ", index);
        addEntry(baseId + index * 3 + 1, "Vector . Y . ", index);
        addEntry(baseId + index * 3 + 2, "Vector . Z . ", index);
      }
      break;
    case PROCEDURAL_CHANNEL_TYPE_MATRIX:
      for (Int32 index = 0; index < itemLength; ++index) {
        addEntry(baseId + index * 12 +  0, "Matrix . Off . X . ", index);
        addEntry(baseId + index * 12 +  1, "Matrix . Off . Y . ", index);
        addEntry(baseId + index * 12 +  2, "Matrix . Off . Z . ", index);
        addEntry(baseId + index * 12 +  3, "Matrix . Off . X . ", index);
        addEntry(baseId + index * 12 +  4, "Matrix . Off . Y . ", index);
        addEntry(baseId + index * 12 +  5, "Matrix . Off . Z . ", index);
        addEntry(baseId + index * 12 +  6, "Matrix . Off . X . ", index);
        addEntry(baseId + index * 12 +  7, "Matrix . Off . Y . ", index);
        addEntry(baseId + index * 12 +  8, "Matrix . Off . Z . ", index);
        addEntry(baseId + index * 12 +  9, "Matrix . Off . X . ", index);
        addEntry(baseId + index * 12 + 10, "Matrix . Off . Y . ", index);
        addEntry(baseId + index * 12 + 11, "Matrix . Off . Z . ", index);
      }
      break;
    case PROCEDURAL_CHANNEL_TYPE_NIL:
    default:
      break;
  }

  return true;
}

/// **************************************************************************
/// **************************************************************************
void CsvEntry::Update(BaseTag* op, const csv_table& table) const
{
  BaseContainer* data = op->GetDataInstance();
  if (!data) return;

  const Bool header = data->GetBool(PROCEDURAL_CSVREADER_HASHEADER);
  const String name = data->GetString(mBaseId + PROCEDURAL_CSVREADER_ENTRY_CHANNELNAME);
  Channel* channel = Channel::Find(op, name);
  if (!channel) return;

  const Int32 lastDcount = data->GetInt32(mBaseId + PROCEDURAL_CSVREADER_ENTRY_LASTDIRTYCOUNT);
  const Int32 dcount = channel->GetDirty(DIRTYFLAGS_DATA);
  if (dcount == lastDcount) return;
  data->SetInt32(mBaseId + PROCEDURAL_CSVREADER_ENTRY_LASTDIRTYCOUNT, dcount);

  const Int32 startIndex = (header ? 1 : 0);
  channel->SetCount(static_cast<Int32>(table.row_count()) - startIndex);
  const Int32 itemLength = channel->GetItemLength();
  if (channel->GetChannelState() != PROCEDURAL_CHANNEL_STATE_INITIALIZED) {
    GePrint("[CSV Reader]: Channel Not Initialized or Error (" + channel->GetName() + ")");
    return;
  }

  CsvTableConverter converter(table);
  const Int32 baseId = mBaseId + PROCEDURAL_CSVREADER_ENTRY_COLUMNSTART;
  Int32 column;

  // Read in the column indices, which is at max 12 * itemLength.
  maxon::BaseArray<Int32> columns;
  columns.Resize(12 * itemLength);
  for (Int32 sub = 0; sub < 12 * itemLength; ++sub)
    columns[sub] = data->GetInt32(baseId + sub) - 1;

  switch (channel->GetChannelType()) { // switch: PROCEDURAL_CHANNEL_TYPE
    case PROCEDURAL_CHANNEL_TYPE_INTEGER: {
      Int32 value;
      for (Int32 row = startIndex; row < table.row_count(); ++row) {
        for (Int32 sub = 0; sub < itemLength; ++sub) {
          column = columns[sub];
          value = converter.Convert<Int32>(&nr::c4d::to_int32, row, column);
          channel->SetElement(value, row - startIndex, sub);
        }
      }
      break;
    }
    case PROCEDURAL_CHANNEL_TYPE_FLOAT: {
      Float value;
      for (Int32 row = startIndex; row < table.row_count(); ++row) {
        for (Int32 sub = 0; sub < itemLength; ++sub) {
          column = columns[sub];
          value = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          channel->SetElement(value, row - startIndex, sub);
        }
      }
      break;
    }
    case PROCEDURAL_CHANNEL_TYPE_VECTOR: {
      Vector value;
      for (Int32 row = startIndex; row < table.row_count(); ++row) {
        for (Int32 sub = 0; sub < itemLength; ++sub) {
          column = columns[sub * 3 + 0];
          value.x = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 1];
          value.y = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 2];
          value.z = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          channel->SetElement(value, row - startIndex, sub);
        }
      }
      break;
    }
    case PROCEDURAL_CHANNEL_TYPE_MATRIX: {
      using namespace c4d_apibridge::M;
      Matrix value;
      for (Int32 row = startIndex; row < table.row_count(); ++row) {
        for (Int32 sub = 0; sub < itemLength; ++sub) {
          column = columns[sub * 3 + 0];
          Moff(value).x = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 1];
          Moff(value).y = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 2];
          Moff(value).z = converter.Convert<Float>(&nr::c4d::to_float, row, column);

          column = columns[sub * 3 + 3];
          Mv1(value).x = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 4];
          Mv1(value).y = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 5];
          Mv1(value).z = converter.Convert<Float>(&nr::c4d::to_float, row, column);

          column = columns[sub * 3 + 6];
          Mv2(value).x = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 7];
          Mv2(value).y = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 8];
          Mv2(value).z = converter.Convert<Float>(&nr::c4d::to_float, row, column);

          column = columns[sub * 3 + 9];
          Mv3(value).x = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 10];
          Mv3(value).y = converter.Convert<Float>(&nr::c4d::to_float, row, column);
          column = columns[sub * 3 + 11];
          Mv3(value).z = converter.Convert<Float>(&nr::c4d::to_float, row, column);

          channel->SetElement(value, row - startIndex, sub);
        }
      }
      break;
    }
    case PROCEDURAL_CHANNEL_TYPE_STRING: {
      for (Int32 row = startIndex; row < table.row_count(); ++row) {
        csv_table::csv_row const& csvData = table.row(row);
        for (Int32 sub = 0; sub < itemLength; ++sub) {
          column = columns[sub];
          String value = tostr(get<std::string>(csvData, sub, ""));
          channel->SetElement(value, row - startIndex, sub);
        }
      }
      break;
    }
    case PROCEDURAL_CHANNEL_TYPE_NIL:
      break;
    default:
      GePrint("[CSV Reader]: Unknown Channel Type (" + channel->GetName() + ")");
      break;
  }
}

/// **************************************************************************
/// **************************************************************************
class CsvReaderPlugin : public TagData
{
  typedef TagData SUPER;
public:

  static NodeData* Alloc() { return NewObjClear(CsvReaderPlugin); }

  CsvReaderPlugin()
    : m_reloadDcount(), m_cycleDcount(), m_cycleContainer(), m_table() { }

  inline BaseTag* Get() { return static_cast<BaseTag*>(SUPER::Get()); }
  inline BaseTag* Get(GeListNode* node) { return static_cast<BaseTag*>(node); }
  inline Int32 GetEntryCount(GeListNode* node)
  {
    if (!node) return 0;
    return GetEntryCount(Get(node)->GetDataInstance());
  }
  inline Int32 GetEntryCount(const BaseContainer* bc)
  {
    if (!bc) return 0;
    Int32 count = bc->GetInt32(PROCEDURAL_CSVREADER_ENTRYCOUNT);
    return clamp<Int32>(count, 0, PROCEDURAL_CSVREADER_MAXENTRIES);
  }
  inline void SetStatus(BaseTag* op, const String& status)
  {
    op->SetParameter(PROCEDURAL_CSVREADER_STATUS, status, DESCFLAGS_SET_0);
  }

  void UpdateTable(BaseTag* op, Bool forceRefresh = false);

  /// ObjectData Overrides

  virtual Bool AddToExecution(BaseTag* op, PriorityList* list) override;
  virtual EXECUTIONRESULT Execute(
    BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt,
    Int32 priority, EXECUTIONFLAGS flags) override;

  /// NodeData Overrides

  virtual Bool Init(GeListNode* node) override;
  virtual Bool GetDParameter(
    GeListNode* node, const DescID& id,
    GeData& data, DESCFLAGS_GET& flags) override;
  virtual Bool GetDEnabling(
    GeListNode* node, const DescID& id, const GeData& data,
    DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) override;
  virtual Bool SetDParameter(
    GeListNode* node, const DescID& id,
    const GeData& data, DESCFLAGS_SET& flags) override;
  virtual Bool GetDDescription(
    GeListNode* node, Description* desc, DESCFLAGS_DESC& flags) override;
  virtual Bool Message(GeListNode* node, Int32 msg, void* pData) override;

private:
  /// Dirty-count for UpdateTable().
  Int32 m_reloadDcount;
  /// Dirty count for the \var m_cycleContainer.
  Int32 m_cycleDcount;
  /// This container will hold the drop-down options.
  BaseContainer m_cycleContainer;
  /// This data structure reads a CSV file and stores it in memory.
  csv_table m_table;
};

/// **************************************************************************
/// **************************************************************************
void CsvReaderPlugin::UpdateTable(BaseTag* op, Bool forceRefresh)
{
  Int32 dcount = op->GetDirty(DIRTYFLAGS_DATA);
  if (!forceRefresh && dcount == m_reloadDcount)
    return;
  m_reloadDcount = dcount;
  goto main;

cleanup:
  m_table.clear();
  m_cycleContainer.FlushAll();
  m_cycleContainer.SetString(0, "---"_s);
  op->SetDirty(DIRTYFLAGS_DESCRIPTION);
  return;

main:
  BaseDocument* doc = op->GetDocument();
  BaseContainer* bc = op->GetDataInstance();
  if (!bc || !doc) {
    this->SetStatus(op, "Memory Error.");
    goto cleanup;
  }

  const Bool hasHeader = bc->GetBool(PROCEDURAL_CSVREADER_HASHEADER);
  const Filename sfn = bc->GetFilename(PROCEDURAL_CSVREADER_FILENAME);
  if (IsEmpty(sfn)) {
    this->SetStatus(op, "Not loaded.");
    goto cleanup;
  }

  const Filename dfn = nr::c4d::find_resource(sfn, doc);
  if (IsEmpty(dfn)) {
    this->SetStatus(op, "File could not be found.");
    goto cleanup;
  }

  bool didReload = false;
  Int32 start = GeGetMilliSeconds();

  bool success = m_table.init(std::to_string(dfn.GetString()), forceRefresh, &didReload);
  print::info("Parsed CSV table in " + tostr(Float(GeGetMilliSeconds() - start) / 1000.0) + "s (%d rows)", (Int32)m_table.row_count());

  Bool rebuildContainer = false;
  dcount = hasHeader;
  if (!success || didReload || m_cycleDcount != dcount)
    rebuildContainer = true;
  m_cycleDcount = dcount;

  if (didReload) {
    String message;
    /*if (!success)
      message = "Error (" + tostr(m_table.GetLastError());
    else*/
    message = "Loaded. Rows: " + tostr((UInt) m_table.row_count()) +
    " Min Cols: " + tostr((UInt) m_table.row_min()) +
    " Max Cols: " + tostr((UInt) m_table.row_max());
    this->SetStatus(op, message);
  }

  if (rebuildContainer) {
    m_cycleContainer.FlushAll();
    m_cycleContainer.SetString(0, "---"_s);
    if (success) {
      if (m_table.row_count() > 0) {
        csv_table::csv_row const& header = m_table.row(0);
        for (Int32 index = 0; index < m_table.row_max(); ++index) {
          String name = (hasHeader && index < header.size() ? tostr(header[index]) : tostr(index));
          m_cycleContainer.SetString(index + 1, name);
        }
      }
    }
    op->SetDirty(DIRTYFLAGS_DESCRIPTION);
  }
}

/// **************************************************************************
/// **************************************************************************
Bool CsvReaderPlugin::AddToExecution(BaseTag* op, PriorityList* list)
{
  if (!op || !list) return false;
  nr::c4d::add_to_execution(op, list, PROCEDURAL_CSVREADER_PRIORITY);
  return true;
}

/// **************************************************************************
/// **************************************************************************
EXECUTIONRESULT CsvReaderPlugin::Execute(
  BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt,
  Int32 priority, EXECUTIONFLAGS flags)
{
  if (!op || !doc)
    return EXECUTIONRESULT_OUTOFMEMORY;
  this->UpdateTable(tag);
  const Bool enabled = nr::c4d::get_param(tag, PROCEDURAL_CSVREADER_ENABLED).GetBool();
  if (enabled) {
    const Int32 count = this->GetEntryCount(tag);
    for (Int32 index = 0; index < count; ++index) {
      CsvEntry entry(index);
      entry.Update(tag, m_table);
    }
  }
  return EXECUTIONRESULT_OK;
}

/// **************************************************************************
/// **************************************************************************
Bool CsvReaderPlugin::Init(GeListNode* node)
{
  BaseTag* op = this->Get(node);
  if (!op || !SUPER::Init(node)) return false;
  BaseContainer* data = op->GetDataInstance();
  if (!data) return false;

  // op->SetDeformMode(true);
  data->SetBool(PROCEDURAL_CSVREADER_ENABLED, true);
  data->SetString(PROCEDURAL_CSVREADER_STATUS, "Not Loaded."_s);
  data->SetFilename(PROCEDURAL_CSVREADER_FILENAME, Filename());
  data->SetBool(PROCEDURAL_CSVREADER_HASHEADER, false);
  data->SetInt32(PROCEDURAL_CSVREADER_ENTRYCOUNT, 0);
  data->SetData(PROCEDURAL_CSVREADER_PRIORITY,
    GeData(CUSTOMGUI_PRIORITY_DATA, DEFAULTVALUE));
  data->SetBool(PROCEDURAL_CSVREADER_ANIMATED, false);
  data->SetBool(PROCEDURAL_CSVREADER_ANIMCACHED, false);
  data->SetTime(PROCEDURAL_CSVREADER_ANIMOFFSET, BaseTime());
  data->SetString(PROCEDURAL_CSVREADER_ANIMSTATUS, "42 frames found."_s);
  return true;
}

/// **************************************************************************
/// **************************************************************************
Bool CsvReaderPlugin::GetDDescription(
  GeListNode* node, Description* desc, DESCFLAGS_DESC& flags)
{
  BaseTag* op = this->Get(node);
  if (!op || !desc) return false;
  if (!desc->LoadDescription(op->GetType())) return false;
  flags |= DESCFLAGS_DESC_LOADED;

  const Int32 entryCount = this->GetEntryCount(op);
  const DescID parentId = PROCEDURAL_CSVREADER_GROUP;
  for (Int32 index = 0; index < entryCount; index++) {
    CsvEntry entry(index);
    entry.FillDescription(op, m_cycleContainer, desc, parentId);
  }
  return true;
}

/// **************************************************************************
/// **************************************************************************
Bool CsvReaderPlugin::GetDEnabling(
    GeListNode* node, const DescID& id, const GeData& data,
    DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc)
{
  if (!node) return false;
  BaseContainer* bc = static_cast<BaseList2D*>(node)->GetDataInstance();
  if (!bc) return false;
  switch (id[-1].id) {
    case PROCEDURAL_CSVREADER_ANIMSTATUS:
    case PROCEDURAL_CSVREADER_ANIMCACHED:
    case PROCEDURAL_CSVREADER_ANIMOFFSET:
      return bc->GetBool(PROCEDURAL_CSVREADER_ANIMATED);
  }
  return SUPER::GetDEnabling(node, id, data, flags, itemdesc);
}

/// **************************************************************************
/// **************************************************************************
Bool CsvReaderPlugin::GetDParameter(
  GeListNode* node, const DescID& id,
  GeData& data, DESCFLAGS_GET& flags)
{
  BaseTag* op = static_cast<BaseTag*>(node);
  if (!op) return false;
  // switch (id[-1].id) {
  // }
  return SUPER::GetDParameter(node, id, data, flags);
}

/// **************************************************************************
/// **************************************************************************
Bool CsvReaderPlugin::SetDParameter(
  GeListNode* node, const DescID& id,
  const GeData& data, DESCFLAGS_SET& flags)
{
  BaseTag* op = static_cast<BaseTag*>(node);
  if (!op) return false;
  // switch (id[-1].id) {
  // }
  return SUPER::SetDParameter(node, id, data, flags);
}

/// **************************************************************************
/// **************************************************************************
Bool CsvReaderPlugin::Message(GeListNode* node, Int32 msg, void* pData)
{
  BaseTag* op = this->Get(node);
  if (!op) return false;
  switch (msg) {
    /// **********************************************************************
    case MSG_DESCRIPTION_COMMAND: {
      BaseContainer* bc = op->GetDataInstance();
      const auto* data = reinterpret_cast<const DescriptionCommand*>(pData);
      if (!bc || !data) return false;
      const Int32 widgetId = GetDescriptionID(data)[-1].id;

      if (widgetId == PROCEDURAL_CSVREADER_ADDENTRY) {
        const Int32 count = this->GetEntryCount(bc);
        if (count < PROCEDURAL_CSVREADER_MAXENTRIES) {
          CsvEntry(count).Init(bc);
          bc->SetInt32(PROCEDURAL_CSVREADER_ENTRYCOUNT, count + 1);
        }
        op->SetDirty(DIRTYFLAGS_DESCRIPTION);
        return true;
      }
      else if (widgetId == PROCEDURAL_CSVREADER_SUBENTRY) {
        const Int32 count = this->GetEntryCount(bc);
        if (count > 0) {
          bc->SetInt32(PROCEDURAL_CSVREADER_ENTRYCOUNT, count - 1);
        }
        op->SetDirty(DIRTYFLAGS_DESCRIPTION);
        return true;
      }
      break;
    }
    /// **********************************************************************
    case MSG_DESCRIPTION_POSTSETPARAMETER: {
      BaseContainer* bc = op->GetDataInstance();
      const auto* data = reinterpret_cast<const DescriptionPostSetValue*>(pData);
      if (!bc || !data || !data->descid) return false;
      const Int32 widgetId = (*data->descid)[-1].id;

      // Reload the CSV data if the filename or "has header" parameter changed.
      if (widgetId == PROCEDURAL_CSVREADER_FILENAME || widgetId == PROCEDURAL_CSVREADER_HASHEADER) {
        UpdateTable(op);
        return true;
      }

      // If the Channel Name of a CsvEntry is changed, we have to update
      // the description ouf our object.
      // Also, if anything changed within the parameter range of a Csv Entry,
      // we have to update our target channel (we set it dirty).
      const Int32 entryIndex = CsvEntry::Revert(widgetId);
      if (entryIndex >= 0) {
        const CsvEntry entry(entryIndex);
        if (widgetId == entry + PROCEDURAL_CSVREADER_ENTRY_LASTDIRTYCOUNT) {
          op->SetDirty(DIRTYFLAGS_DESCRIPTION);
          return true;
        }
        const String channelName = bc->GetString(entry + PROCEDURAL_CSVREADER_ENTRY_CHANNELNAME);
        Channel* channel = Channel::Find(op, channelName);
        if (channel) channel->SetDirty(DIRTYFLAGS_DATA);
      }
      break;
    }
    /// **********************************************************************
    case MSG_GETREALCAMERADATA: {
      // TODO: This message is sent frequently when a parameter is updated in
      // the attributes manager. We could use it to set DIRTYFLAGS_DESCRIPTION
      // dirty when the channels referenced by this CsvReader have changed.
      break;
    }
    /// **********************************************************************
    case MSG_DESCRIPTION_GETINLINEOBJECT: {
      const BaseContainer* bc = op->GetDataInstance();
      const auto* data = reinterpret_cast<DescriptionInlineObjectMsg*>(pData);
      if (!bc || !data || !data->id || !data->objects) return false;
      const Int32 widgetId = (*data->id)[-1].id;

      // Return the referenced channel for the "Target Channel" parameter of
      // each entry in the Csv Reader.
      const Int32 entryIndex = CsvEntry::Revert(widgetId);
      if (entryIndex >= 0) {
        const CsvEntry entry(entryIndex);
        if (widgetId == entry + PROCEDURAL_CSVREADER_ENTRY_CHANNELNAME) {
          Channel* channel = Channel::Find(op, bc->GetString(widgetId));
          if (channel)
            data->objects->Append(channel);
          return true;
        }
      }
      break;
    }
  }

  return SUPER::Message(node, msg, pData);
}

/// **************************************************************************
/// **************************************************************************
Bool nr::procedural::RegisterCsvReaderPlugin()
{
  nr::c4d::auto_bitmap const icon("res/icons/csv_reader.png");
  Int32 const flags = TAG_VISIBLE | TAG_EXPRESSION;
  Int32 const disklevel = 0;
  if (!RegisterTagPlugin(
      PROCEDURAL_CSVREADER_ID,
      "CSV Reader"_s,
      flags,
      CsvReaderPlugin::Alloc,
      "nrprocedural_csvreader"_s,
      icon,
      disklevel))
    return false;
  return true;
}

/// **************************************************************************
/// **************************************************************************
void nr::procedural::UnloadCsvReaderPlugin()
{
}
