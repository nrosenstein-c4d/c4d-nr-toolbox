// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.
/*!
 * This file implements a multithreaded 2explode segments2 command for
 * Cinema 4D Polygon Objects.
 */

#define PRINT_PREFIX "[nr-toolbox/Explode]: "

#include <algorithm>
#include <c4d.h>
#include "res/c4d_symbols.h"
#include "misc/detect_components.h"
#include "misc/print.h"
#include "misc/raii.h"
#include "misc/utils.h"
#include "menu.h"

static Int32 const PLUGIN_ID = 1037481;

namespace explode {

//============================================================================
/*!
 * Set all items of the specified #array to #val.
 */
//============================================================================
template <class C, typename T>
static inline void FillArray(C& array, T const& val) {
  for (T& item : array) {
    item = val;
  }
}

//============================================================================
/*!
 * Binary search on the #array with the specified #size for #val.
 * Returns the index of the value or -1 if it could not be found.
 */
//============================================================================
template <class C, typename T>
static inline Int32 BinarySearch(C& array, Int32 size, T const& val) {
  Int32 left = 0, right = size - 1;
  while (left <= right) {
    Int32 pivot = (left + right) / 2;
    T const& ref = array[pivot];
    if (ref == val) return pivot;
    else if (ref < val) left = pivot + 1;
    else right = pivot - 1;
  }
  return -1;
}

//============================================================================
/*!
 * Error codes for the #Error structure.
 */
//============================================================================
enum ERROR {
  ERROR_NONE,
  ERROR_BREAK,
  ERROR_MEMORY,
  ERROR_UNKNOWN,
  ERROR_INVALID,
};

//============================================================================
/*!
 * Structure for error information.
 */
//============================================================================
struct Error {
  ERROR code;
  String message;

  Error() : code(ERROR_NONE), message() { }
  Error(ERROR code, String const& message) : code(code), message(message) { }
  operator Bool () const { return code != ERROR_NONE; }
  static Error None() { return Error{ERROR_NONE, ""}; }
  static Error Break() { return Error{ERROR_BREAK, ""}; }
  static Error Memory(String const& msg) { return Error{ERROR_MEMORY, msg}; }
  static Error Unknown(String const& msg) { return Error{ERROR_UNKNOWN, msg}; }
  static Error Invalid(String const& msg) { return Error{ERROR_INVALID, msg}; }

  String Format() const {
    String result = print::tostr(code);
    if (message.Content()) result += ": " + message;
    return result;
  }
};

//============================================================================
/*!
 * Structure representing the data of a VertexMap tag.
 */
//============================================================================
struct VertexWeightMap {
  String name;
  Float32 const* weights;
};

//============================================================================
/*!
 * @struct VertexColorMap
 *
 * Structure representing the data of a VertexColor tag. The
 * struct is only available if #HAVE_VERTEXCOLOR is defined.
 */
//============================================================================
#ifdef HAVE_VERTEXCOLOR
struct VertexColorMap {
  String name;
  Bool per_point;
  ConstVertexColorHandle handle;
};
#endif

//============================================================================
/*!
 * Structure representing the data of a UVW tag.
 */
//============================================================================
struct UVWMap {
  String name;
  ConstUVWHandle handle;
};

//============================================================================
/*!
 * This structure contains all the initial information for computing
 * the polygon groups from a single mesh.
 */
//============================================================================
struct GroupData {

  //==========================================================================
  /*!
   * The origianl #PolygonObject.
   */
  //==========================================================================
  PolygonObject* op;

  //==========================================================================
  /*!
   * Number of total vertices in the original object. Read-only.
   */
  //==========================================================================
  Int32 vcnt;

  //==========================================================================
  /*!
   * Number of total faces in the original object. Read-only.
   */
  //==========================================================================
  Int32 fcnt;

  //==========================================================================
  /*!
   * The original object's vertex data with #vcnt elements.
   */
  //==========================================================================
  Vector const* vdata;

  //==========================================================================
  /*!
   * The original object's face data with #fcnt elements.
   */
  //==========================================================================
  CPolygon const* fdata;

  //==========================================================================
  /*!
   * The #Neighbor structure of the original object.
   */
  //==========================================================================
  Neighbor nb;

  //==========================================================================
  /*!
   * The number of objects in which the single source object is split
   * into. Read-only.
   */
  //==========================================================================
  Int32 groups;

  //==========================================================================
  /*!
   * Maps the vertex indices of the original object to their group,
   * i.e. the index of the object that the vertex is assigned to.
   */
  //==========================================================================
  maxon::BaseArray<Int32> vgroups;

  //==========================================================================
  /*!
   * An integer for each group that specifies the number of vertices
   * that this group has assigned.
   */
  //==========================================================================
  maxon::BaseArray<Int32> vcounts;

  //==========================================================================
  /*!
   * Maps the face indices of the original object to their group, i.e.
   * the index of the object that the face is assigned to.
   */
  //==========================================================================
  maxon::BaseArray<Int32> fgroups;

  //==========================================================================
  /*!
   * An integer for each group that specifies the number of faces
   * that this group has assigned.
   */
  //==========================================================================
  maxon::BaseArray<Int32> fcounts;

  //==========================================================================
  /*!
   * A list of #VertexMap structures that contain the information
   * of all the vertex maps on the original object.
   */
  //==========================================================================
  maxon::BaseArray<VertexWeightMap> weight_maps;

  //==========================================================================
  /*!
   * A list of #UVWMap structures tht contain the information of
   * all uvw maps on the original object.
   */
  //==========================================================================
  maxon::BaseArray<UVWMap> uvw_maps;

  //==========================================================================
  /*!
   * @member maxon::BaseArray<VertexvColor> color_maps
   *
   * A list of #VertexColor structures that contain the informatin
   * of all the vertex color maps on the original object. Only
   * available when #HAVE_VERTEXCOLOR is defined.
   */
  //==========================================================================
  #ifdef HAVE_VERTEXCOLOR
  maxon::BaseArray<VertexColorMap> color_maps;
  #endif

  //==========================================================================
  /*!
   * Callback type for progress information on the #Init() method.
   */
  //==========================================================================
  typedef std::function<void(Float, Int32)> ProgressCallback;

  //==========================================================================
  /*!
   * Initialize the #GroupData from a #PolygonObject.
   */
  //==========================================================================
  Error Init(PolygonObject* op, BaseThread* bt=nullptr, ProgressCallback const& p={});
};

//============================================================================
//============================================================================
Error GroupData::Init(PolygonObject* op, BaseThread* bt, ProgressCallback const& p) {
  if (!op)
    return Error::Invalid("op must not be nullptr");

  this->op = op;
  this->vcnt = op->GetPointCount();
  this->fcnt = op->GetPolygonCount();
  this->vdata = op->GetPointR();
  this->fdata = op->GetPolygonR();
  this->groups = 0;
  if (!this->vdata || !this->fdata)
    return Error::Unknown("vertex/face data could not be read");
  if (!this->nb.Init(this->vcnt, this->fdata, this->fcnt, nullptr))
    return Error::Unknown("Neighbor could not be initialized");
  if (!this->vgroups.Resize(vcnt))
    return Error::Memory("vgroups could not be resized");
  if (!this->fgroups.Resize(fcnt))
    return Error::Memory("fgroups could not be resized");

  /* Initialize all group assignments with -1 (no group). */
  static Int32 const NOGROUP = -1;
  FillArray(this->vgroups, NOGROUP);
  FillArray(this->fgroups, NOGROUP);

  /* Go through all connected faces in a pseudo-recursive fashion and
   * assign each to the same group. Skip faces we already tagged. */
  maxon::BaseArray<Int32> fstack;
  for (Int32 i = 0; i < this->fcnt; ++i) {
    if (utils::TestBreak<20>(i, bt)) return Error::Break();
    if (this->fgroups[i] != NOGROUP) {
      continue;
    }
    if (p) p(i / Float(this->fcnt), this->groups);
    fstack.Append(i);
    while (fstack.GetCount() > 0) {
      Int32 fi; fstack.Pop(&fi);
      if (this->fgroups[fi] != NOGROUP)
        continue;  /* We already processed this face. */

      PolyInfo* info = nb.GetPolyInfo(fi);
      if (!info)
        continue;

      CPolygon const& f = fdata[fi];
      this->vgroups[f.a] = this->groups;
      this->vgroups[f.b] = this->groups;
      this->vgroups[f.c] = this->groups;
      this->vgroups[f.d] = this->groups;
      this->fgroups[fi] = this->groups;

      /* Add all neighbouring faces to the stack. */
      for (Int32 j = 0; j < 4; ++j) {
        if (info->face[j] != NOTOK)
          fstack.Append(info->face[j]);
      }
    }
    ++this->groups;
  }

  /* Calculate the number of vertices & faces per group. */
  if (!this->vcounts.Resize(this->groups))
    return Error::Memory("vcounts could not be resized");
  if (!this->fcounts.Resize(this->groups))
    return Error::Memory("fcounts could not be resized");
  for (Int32 i = 0; i < vcnt; ++i)
    this->vcounts[this->vgroups[i]] += 1;
  for (Int32 i = 0; i < fcnt; ++i)
    this->fcounts[this->fgroups[i]] += 1;

  if (utils::TestBreak(bt)) return Error::Break();

  /* Collect the variable tags that we support copying over. */
  for (BaseTag* tag = op->GetFirstTag(); tag; tag = tag->GetNext()) {
    switch (tag->GetType()) {
      case Tvertexmap: {
        VertexWeightMap map;
        map.name = tag->GetName();
        map.weights = static_cast<VertexMapTag*>(tag)->GetDataAddressR();
        this->weight_maps.Append(map);
      } break;

      #ifdef HAVE_VERTEXCOLOR
      case Tvertexcolor: {
        VertexColorMap map;
        map.name = tag->GetName();
        map.per_point = static_cast<VertexColorTag*>(tag)->IsPerPointColor();
        map.handle = static_cast<VertexColorTag*>(tag)->GetDataAddressR();
        this->color_maps.Append(map);
      } break;
      #endif // HAVE_VERTEXCOLOR

      case Tuvw: {
        UVWMap map;
        map.name = tag->GetName();
        map.handle = static_cast<UVWTag*>(tag)->GetDataAddressR();
        this->uvw_maps.Append(map);
      } break;
    }
  }

  return Error::None();
}

//============================================================================
/*!
 * Output structure for a single polygon group after #BuildGeometry()
 * was used with an initialized #GroupData.
 */
//============================================================================
struct ObjectData {

  //==========================================================================
  /*!
   * The output #PolygonObject. Use #op::Release() to release the object
   * from automatic memory management and take ownership.
   */
  //==========================================================================
  raii::AutoFree<PolygonObject> op;

  //==========================================================================
  /*!
   * Maps the new vertex indices to the indices in the old geometry.
   */
  //==========================================================================
  maxon::BaseArray<Int32> vmap;

  //==========================================================================
  /*!
   * Maps the new face indices to the indices in the old geometry.
   */
  //==========================================================================
  maxon::BaseArray<Int32> fmap;

  //==========================================================================
  /*!
   * Required during the geometry building process. Ends up with the number
   * of vertices of the object. Used to find at which index to place the
   * next vertex of the object.
   */
  //==========================================================================
  Int32 vfilled;

  //==========================================================================
  /*!
   * Required during the geometry building process. Ends up with the number
   * of faces of the object. Used to find at which index to place the next
   * face of the object.
   */
  //==========================================================================
  Int32 ffilled;

  //==========================================================================
  /*!
   * Actual vertex count.
   */
  //==========================================================================
  Int32 vcnt;

  //==========================================================================
  /*!
   * Actual face count.
   */
  //==========================================================================
  Int32 fcnt;

  //==========================================================================
  /*!
   * The output vertex buffer of #op.
   */
  //==========================================================================
  Vector* vdata;

  //==========================================================================
  /*!
   * The output face buffer of #op.
   */
  //==========================================================================
  CPolygon* fdata;

  //==========================================================================
  /*!
   * Empty constructor.
   */
  //==========================================================================
  ObjectData()
  : vfilled(0), ffilled(0), vcnt(0), fcnt(0), vdata(nullptr), fdata(nullptr) { }

  //==========================================================================
  /*!
   * Move constructor. Required for MSVC as it does not support automatic
   * generation of move constructor of complex types up until VS 2015.
   */
  //==========================================================================
  ObjectData(ObjectData&& other)
  : op(std::move(op)), vfilled(other.vfilled), ffilled(other.ffilled),
    vcnt(other.vcnt), fcnt(other.fcnt), vmap(std::move(other.vmap)),
    fmap(std::move(other.fmap)), vdata(other.vdata), fdata(other.fdata)
  { }

  //==========================================================================
  /*!
   * Initialize the #ObjectData with the specified number of vertices
   * and faces. Returns true if the initialization was successful, false
   * if a memory error occured.
   */
  //==========================================================================
  Bool Init(Int32 vcnt, Int32 fcnt) {
    this->op = PolygonObject::Alloc(vcnt, fcnt);
    if (!this->op) return false;
    if (!this->vmap.Resize(vcnt)) return false;
    if (!this->fmap.Resize(fcnt)) return false;
    this->vfilled = 0;
    this->ffilled = 0;
    this->vcnt = vcnt;
    this->fcnt = fcnt;
    this->vdata = this->op->GetPointW();
    this->fdata = this->op->GetPolygonW();
    if (!this->vdata || !this->fdata) return false;
    return true;
  }

  //==========================================================================
  /*!
   * Given a #VertexWeightMap structure initialized with data from the
   * source geometry (ideally obtained from the #GroupData), copies all
   * the relevant information to this object.
   *
   * This function is called from #BuildGeometry() and likely does not
   * need to be called manually at any point.
   */
  //==========================================================================
  Error CopyVertexWeightMap(VertexWeightMap const& map) {
    if (!this->op) return Error::Invalid("ObjectData not initialized");
    VertexMapTag* tag = VertexMapTag::Alloc(this->vcnt);
    if (!tag) return Error::Memory("vertex map could not be allocated");

    tag->SetName(map.name);
    this->op->InsertTag(tag);
    Float32* weights = tag->GetDataAddressW();
    if (!weights) return Error::Memory("vertex map data address error");

    for (Int32 j = 0; j < vcnt; ++j)
      weights[j] = map.weights[vmap[j]];

    return Error::None();
  }

  //==========================================================================
  /*!
   * @member Error CopyVertexColorMap(VertexColorMap const& map)
   * @note This function is only available when #HAVE_VERTEXCOLOR
   *   is defined.
   *
   * Given a #VertexColorMap structure initialized with data from the
   * source geometry (ideally obtained from the #GroupData), copies all
   * the relevant information to this object.
   *
   * This function is called from #BuildGeometry() and likely does not
   * need to be called manually at any point
   */
  //==========================================================================
  #ifdef HAVE_VERTEXCOLOR
  Error CopyVertexColorMap(VertexColorMap const& map) {
    if (!this->op) return Error::Invalid("ObjectData not initialized");
    VertexColorTag* tag = VertexColorTag::Alloc(map.per_point ? vcnt : fcnt);
    if (!tag) return Error::Memory("vertex color map could not be allocated");

    tag->SetName(map.name);
    this->op->InsertTag(tag);
    VertexColorHandle handle = tag->GetDataAddressW();
    if (!handle) return Error::Memory("vertex color map data address error");

    if (map.per_point) {
      // Copy the per vertex colors.
      for (Int32 j = 0; j < vcnt; ++j) {
        Int32 const pi = vmap[j];
        // TODO: Not sure if passing nullptr is valid, but what CPolygon to use??
        //       We're in per-point mode, nothing to do with polygons.
        Vector4d32 c = VertexColorTag::Get(map.handle, nullptr, nullptr, pi);
        VertexColorTag::Set(handle, nullptr, nullptr, j, c);
      }
    }
    else {
      // Copy the per polygon vertex colors.
      for (Int32 j = 0; j < fcnt; ++j) {
        VertexColorStruct s;
        VertexColorTag::Get(map.handle, fmap[j], s);
        VertexColorTag::Set(handle, j, s);
      }
    }
    return Error::None();
  }
  #endif // HAVE_VERTEXCOLOR

  //==========================================================================
  /*!
   * Given a #UVWMap structure initialized with data from the source geometry
   * (ideally obtained from the #GroupData), copies all the relevant
   * information to this object.
   *
   * This function is called from #BuildGeometry() and likely does not
   * need to be called manually at any point.
   */
  //==========================================================================
  Error CopyUVWMap(UVWMap const& map) {
    if (!this->op) return Error::Invalid("ObjectData not initialized");
    UVWTag* tag = UVWTag::Alloc(fcnt);
    if (!tag) return Error::Memory("uvw tag could not be allocated");

    tag->SetName(map.name);
    this->op->InsertTag(tag);
    UVWHandle handle = tag->GetDataAddressW();
    if (!handle) return Error::Memory("uvw tag data address error");

    for (Int32 j = 0; j < fcnt; ++j) {
      UVWStruct s;
      UVWTag::Get(map.handle, fmap[j], s);
      UVWTag::Set(handle, j, s);
    }

    return Error::None();
  }

};

//============================================================================
/*!
 * Callback type for #BuildGeometry(). The callback is only called once in a
 * while when a step of the geometry building is completed.
 */
//============================================================================
typedef std::function<void(Float)> BuildGeometryProgressCallback;

//============================================================================
/*!
 * Build separate #PolygonObjects from an initialized #GroupData.
 *
 * @param data The input #GroupData. Must be initialized.
 * @param objects The output array of #ObjectData.
 */
//============================================================================
Error BuildGeometry(
  GroupData const& data, maxon::BaseArray<ObjectData>& objects,
  BaseThread* bt=nullptr, BuildGeometryProgressCallback const& p={})
{
  static Float const STEPS = 5.0;
  if (p) p(0 / STEPS);

  if (!objects.Resize(data.groups))
    return Error::Memory("objects could not be resized");

  /* Initialize all ObjectData elements. */
  for (Int32 i = 0; i < data.groups; ++i) {
    if (utils::TestBreak<20>(i, bt)) return Error::Break();
    if (!objects[i].Init(data.vcounts[i], data.fcounts[i]))
      return Error::Memory("ObjectData could not be initialized");
  }

  /* Iterate over all vertices of the source geometry and fill the vertex
   * buffers of the respective output objects. */
  if (p) p(1 / STEPS);
  for (Int32 i = 0; i < data.vcnt; ++i) {
    if (utils::TestBreak<20>(i, bt)) return Error::Break();
    Int32 const gi = data.vgroups[i];
    ObjectData& od = objects[gi];
    od.vdata[od.vfilled] = data.vdata[i];
    od.vmap[od.vfilled] = i;
    ++od.vfilled;
  }

  /* Iterate over all faces of the source geometry and fill the face buffers
   * of the respective output objects. */
  if (p) p(2 / STEPS);
  for (Int32 i = 0; i < data.fcnt; ++i) {
    if (utils::TestBreak<20>(i, bt)) return Error::Break();
    Int32 const gi = data.fgroups[i];
    ObjectData& od = objects[gi];

    /* Reduce vertex indices in the polygon. */
    CPolygon face  = data.fdata[i];
    face.a = BinarySearch(od.vmap, od.vmap.GetCount(), face.a);
    face.b = BinarySearch(od.vmap, od.vmap.GetCount(), face.b);
    face.c = BinarySearch(od.vmap, od.vmap.GetCount(), face.c);
    face.d = BinarySearch(od.vmap, od.vmap.GetCount(), face.d);
    CriticalAssert(face.a != -1 && face.b != -1 && face.c != -1 && face.d != -1);

    od.fdata[od.ffilled] = face;
    od.fmap[od.ffilled] = i;
    ++od.ffilled;
  }

  /* Copy variable data like vertex weights, colors and UVW. */
  if (p) p(3 / STEPS);
  for (ObjectData& obj : objects) {
    Error err;

    /* Vertex weight maps. */
    for (VertexWeightMap const& map : data.weight_maps) {
      err = obj.CopyVertexWeightMap(map);
      if (err) return err;
    }

    #ifdef HAVE_VERTEXCOLOR
    /* Vertex color maps. */
    for (VertexColorMap const& map : data.color_maps) {
      err = obj.CopyVertexColorMap(map);
      if (err) return err;
    }
    #endif

    /* UVW maps. */
    for (UVWMap const& map : data.uvw_maps) {
      err = obj.CopyUVWMap(map);
      if (err) return err;
    }
  }

  /* Update the objects. */
  if (p) p(4 / STEPS);
  Int32 i = 0;
  for (ObjectData& obj : objects) {
    if (utils::TestBreak<20>(i, bt)) return Error::Break();
    obj.op->Message(MSG_UPDATE);
  }

  /* Done. */
  if (p) p(5 / STEPS);
  return Error::None();
}

//============================================================================
/*!
 * Implements the Explode command-
 */
//============================================================================
class ExplodeCommand : public CommandData {
public:

  // CommandData Overrides

  //==========================================================================
  //==========================================================================
  virtual Bool Execute(BaseDocument* doc) override {
    BaseObject* op = doc->GetActiveObject();
    if (!op || !op->IsInstanceOf(Opolygon))
      return false;

    Error err;
    utils::Timer timer;

    StatusSetSpin();
    SetMousePointer(MOUSE_BUSY);
    raii::RAII cleanup([]() {
      StatusClear();
      SetMousePointer(MOUSE_NORMAL);
    });

    /* Phase 1: Find all polygon groups. */
    StatusSetText(GeLoadString(IDS_EXPLODE_FINDING_GROUPS));
    GroupData group_data;
    err = group_data.Init(ToPoly(op));
    if (err) {
      MessageDialog(err.Format());
      return true;
    }

    /* Phase 2: Build geometry. */
    StatusSetText(GeLoadString(IDS_EXPLODE_BUILDING_GEOMETRY));
    maxon::BaseArray<ObjectData> objects;
    err = BuildGeometry(group_data, objects);
    if (err) {
      MessageDialog(err.Format());
      return true;
    }

    /* Phase 3: Insert new objets into the scene. */
    StatusSetSpin();
    StatusSetText(GeLoadString(IDS_EXPLODE_INSERTING_OBJECTS));
    raii::AutoFree<BaseObject> null = BaseObject::Alloc(Onull);
    if (!null) {
      MessageDialog("Null-Object could not be allocated");
      return true;
    }
    for (ObjectData& data : objects)
      data.op.Release()->InsertUnder(null);
    null->SetMl(op->GetMg());
    doc->InsertObject(null, nullptr, nullptr);
    doc->StartUndo();
    doc->AddUndo(UNDOTYPE_NEW, null);
    doc->EndUndo();
    null.Release();

    /* Done! */
    print::info("Exploded", group_data.fcnt, "polygons to", group_data.groups,
      "groups in", timer.Sum() / 1000.0, "seconds.");
    EventAdd();
    return true;
  }

  //==========================================================================
  //==========================================================================
  virtual Int32 GetState(BaseDocument* doc) override {
    BaseObject* op = doc->GetActiveObject();
    if (op && op->IsInstanceOf(Opolygon))
      return CMD_ENABLED;
    return 0;
  }

};

} // namespace explode

//============================================================================
//============================================================================
namespace print {
String tostr(explode::ERROR c) {
  using namespace explode;
  switch (c) {
    case ERROR_NONE: return "No Error";
    case ERROR_BREAK: return "User Break";
    case ERROR_MEMORY: return "Memory Error";
    case ERROR_UNKNOWN: return "Unknown Error";
    case ERROR_INVALID: return "Invalid Input Data";
    default: return "[Very Unknown Error]";
  }
}
} // namespace print

//============================================================================
//============================================================================
Bool RegisterExplodeCommand() {
  menu::root().AddPlugin(PLUGIN_ID);
  using explode::ExplodeCommand;
  String const name = GeLoadString(IDS_EXPLODE_NAME);
  String const help = GeLoadString(IDS_EXPLODE_HELP);
  Int32 const info = PLUGINFLAG_HIDEPLUGINMENU;
  raii::AutoBitmap icon("icons/explode.png");
  return RegisterCommandPlugin(PLUGIN_ID, name, info, icon, help, NewObj(ExplodeCommand));
}
