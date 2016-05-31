/// Copyright (C) 2015 Niklas Rosenstein
/// All rights reserved.

#ifndef NRPROCEDURAL_UTILS_DESCRIPTIONHELPER_H_
#define NRPROCEDURAL_UTILS_DESCRIPTIONHELPER_H_

#include <c4d.h>
#include <lib_description.h>

/// ---------------------------------------------------------------------------
/// This utility class wraps a Description and makes it easier to
/// create description parameters from code.
/// ---------------------------------------------------------------------------
class DescriptionHelper
{
public:
  DescriptionHelper(Description* desc)
    : m_desc(desc), m_singleid(desc->GetSingleDescID()) {}
  ~DescriptionHelper() {}

  inline Bool CheckSingleID(const DescID& id)
  {
    return !m_singleid || id.IsPartOf(*m_singleid, nullptr);
  }

  inline DescID MakeButton(
    Int32 id, const DescID& parentId, const String& name,
    const String& shortName)
  {
    DescID cid = DescLevel(id, DTYPE_BUTTON, 0);
    if (this->CheckSingleID(cid)) {
      BaseContainer bc = GetCustomDataTypeDefault(DTYPE_BUTTON);
      bc.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_BUTTON);
      bc.SetString(DESC_NAME, name);
      if (shortName.Content())
        bc.SetString(DESC_SHORT_NAME, shortName);
      m_desc->SetParameter(cid, bc, parentId);
    }
    return cid;
  }

  inline DescID MakeGroup(
    Int32 id, const DescID& parentId, const String& name,
    const String& shortName, Int32 columns = 1, Bool defaultOpen = false)
  {
    DescID cid = DescLevel(id, DTYPE_GROUP, 0);
    if (this->CheckSingleID(cid)) {
      BaseContainer bc = GetCustomDataTypeDefault(DTYPE_GROUP);
      bc.SetString(DESC_NAME, name);
      if (shortName.Content())
        bc.SetString(DESC_SHORT_NAME, shortName);
      bc.SetInt32(DESC_DEFAULT, defaultOpen);
      bc.SetInt32(DESC_COLUMNS, columns);
      m_desc->SetParameter(cid, bc, parentId);
    }
    return cid;
  }

  inline DescID MakeSeparator(Int32 id, const DescID& parentId, Bool line)
  {
    DescID cid = DescLevel(id, DTYPE_SEPARATOR, 0);
    if (this->CheckSingleID(cid)) {
      BaseContainer bc = GetCustomDataTypeDefault(DTYPE_SEPARATOR);
      bc.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_SEPARATOR);
      bc.SetBool(DESC_SEPARATORLINE, line);
      m_desc->SetParameter(cid, bc, parentId);
    }
    return cid;
  }

private:
  Description* const m_desc;
  const DescID* const m_singleid;
};

#endif // NRPROCEDURAL_UTILS_DESCRIPTIONHELPER_H_
