// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.
/*!
 * The Instance Resolver was originally developed for V-RayForC4D.
 * It searches for duplicate point/polygon objects and replaces them
 * with render instances.
 */

#define PRINT_PREFIX "[nr-toolbox/Resolve Duplicates]: "

#include <c4d.h>
#include <c4d_apibridge.h>
#include <NiklasRosenstein/math/matrix.hpp>
#include "res/c4d_symbols.h"
#include "misc/print.h"
#include "misc/raii.h"
#include "misc/utils.h"
#include "menu.h"

static Int32 const PLUGIN_ID = 1037480;

namespace nr { using namespace niklasrosenstein; }

namespace resolve_duplicates {

//============================================================================
/*!
 * A 4x4 matrix we use in the Cinema 4D context.
 */
//============================================================================
typedef nr::matrix<4, 4, Float> m44;

//============================================================================
/*!
 * Write a Cinema 4D #Vector #c into a column of a 4x4 matrix.
 */
//============================================================================
inline void WriteColumn(m44& mat, size_t col, Vector const& c, Float first=1.0) {
  mat[0][col] = first;
  mat[1][col] = c.x;
  mat[2][col] = c.y;
  mat[3][col] = c.z;
}

//============================================================================
/*!
 * Forward-declaration for casts to Cinema 4D #Matrix objects.
 */
//============================================================================
template <typename T> Matrix cast(T const&);

//============================================================================
/*!
 * Converts the specified #mat to a Cinema 4D #Matrix object.
 */
//============================================================================
template <>
inline Matrix cast<m44>(m44 const& mat) {
  return Matrix(
    Vector(mat[1][0], mat[2][0], mat[3][0]),
    Vector(mat[1][1], mat[2][1], mat[3][1]),
    Vector(mat[1][2], mat[2][2], mat[3][2]),
    Vector(mat[1][3], mat[2][3], mat[3][3]));
}

//============================================================================
/*!
 * Find a linear independent 4x4 matrix from a point set and also return
 * an array of 4 indices of the points that are used in the resulting matrix.
 */
//============================================================================
inline Bool FindIndepMatrix(
  Vector const* points, Int32 count, m44& mat, Int32* verts)
{
  if (count < 4) return false;

  // Initialize the matrix with the first 4 points.
  for (Int32 i = 0; i < 4; ++i) {
    WriteColumn(mat, i, points[i]);
    verts[i] = i;
  }

  // Exchange the columns until the matrix determinant is non-zero.
  for (Int32 index = 4; index < count; ++index) {
    for (Int32 j = 0; j < 4; ++j) {
      if (mat.indep()) return true;
      // Swap the first with the current column.
      std::swap(verts[0], verts[j]);
      mat.swapcol(0, j);
      // Write the column and update the vertex index.
      WriteColumn(mat, 0, points[index]);
      verts[0] = index;
    }
  }

  return mat.indep();
}

//============================================================================
/*!
 * Given two sets of at least points, calculates a matrix that transforms
 * the first set into the second set. Returns true if such a matrix
 * was found, false if not. The resulting matrix is written to #out.
 *
 * Note: To find the correct transformation matrix between two meshes,
 * you should choose points that are not linear dependent!
 */
//============================================================================
inline Bool FindTransformationMatrix(
  Vector const* origin, Vector const* target, Int32 count, Matrix* out)
{
  if (count < 4) return false;

  // Find a linear independent matrix from the origin point set.
  Int32 verts[4];
  m44 m_origin;
  if (!FindIndepMatrix(origin, count, m_origin, verts)) {
    return false;
  }

  // Use the same points from the target point set.
  m44 m_target;
  for (Int32 i = 0; i < 4; ++i) {
    WriteColumn(m_target, i, target[verts[i]]);
  }

  // Re-use one of the matrix objects for space efficiency.
  m44 res = m_target * m_origin.inv();
  *out = cast<>(res);
  return true;
}

//============================================================================
/*!
 * @return true if #x is near zero so that it can be considered zero,
 *   otherwise false.
 */
//============================================================================
static Bool NearZero(Float x) {
  return maxon::Abs(x) < 1.0e-8;
}

//============================================================================
/*!
 * Computes a checksum for a #PolygonObject. If the specified #PointObject
 * can not be casted into a #PolygonObject, this function returns zero.
 */
//============================================================================
static Int32 CalcTopologyChecksum(PointObject* op) {
  if (!op->IsInstanceOf(Opolygon))
    return 0;
  CPolygon const* polys = ToPoly(op)->GetPolygonR();
  Int32 polyCount = ToPoly(op)->GetPolygonCount();
  Int32 sum = 0;
  for (Int32 index = 0; index < polyCount; ++index) {
    sum += polys[index].a + polys[index].b + polys[index].c + polys[index].d;
  }
  return sum + op->GetPointCount();;
}

//============================================================================
/*!
 * Holds checksum information for an object that allows us to efficiently
 * rule out whether two objects could be equal before any more in-depth
 * checks.
 */
//============================================================================
struct ObjectInfo {
  PointObject* op;  // Pointer to the actual object
  Int32 type;  // Object type plugin ID
  Int32 pointChecksum;  // Number of points in the object
  Int32 topologyChecksum;  // Checksum of the mesh topology
  Float phongAngle;
  ObjectInfo* link;  // Link to another #ObjectInfo if the object has been instantiated already
  Bool isTarget;  // True if this object is the target of another instance

  //==========================================================================
  //==========================================================================
  ObjectInfo(PointObject* op) : op(op), type(op->GetType()), link(nullptr), isTarget(false) {
    this->pointChecksum = op->GetPointCount();
    this->topologyChecksum = CalcTopologyChecksum(op);
    this->phongAngle = utils::GetPhongAngle(op);
  }

  //==========================================================================
  //==========================================================================
  bool operator == (ObjectInfo const& other) const {
    return this->op == other.op;
  }

  //==========================================================================
  //==========================================================================
  bool Equals(ObjectInfo const& other, Matrix& trm, Float tolerance=0.1) const {
    // For better performance, compare the checksums before any extensive testing.
    if (this->type != other.type) { print::debug("  - type mismatch"); return false; }
    if (this->topologyChecksum != other.topologyChecksum ) return false;
    if (this->pointChecksum != other.pointChecksum) return false;

    // Make sure the phong angle of both objects match.
    if (!NearZero(this->phongAngle - other.phongAngle)) return false;

    // Get the point and polygon count (if applicable).
    Int32 pcnt = this->op->GetPointCount();
    Vector const* points = this->op->GetPointR();
    Vector const* otherPoints = other.op->GetPointR();
    if (pcnt != other.op->GetPointCount()) return false;
    if (!points || !otherPoints) return false;

    Int32 fcnt = 0;
    CPolygon const* polys = nullptr;
    CPolygon const* otherPolys = nullptr;
    if (this->op->IsInstanceOf(Opolygon)) {
      if (!other.op->IsInstanceOf(Opolygon)) return false;
      fcnt = ToPoly(this->op)->GetPolygonCount();
      polys = ToPoly(this->op)->GetPolygonR();
      otherPolys = ToPoly(other.op)->GetPolygonR();
      if (ToPoly(other.op)->GetPolygonCount() != fcnt) return false;
      if (!polys || !otherPolys) return false;
    }
    else if (other.op->IsInstanceOf(Opolygon)) return false;

    if (!FindTransformationMatrix(points, otherPoints, pcnt, &trm))
      return false;

    // Check if all points match the tolerance.
    for (Int32 i = 0; i < pcnt; ++i) {
      Vector p = trm * points[i];
      if ((otherPoints[i] - p).GetSquaredLength() > tolerance)
        return false;
    }

    // Verify the topology is the same.
    if (polys && otherPolys) {
      // The order of the polygons doesn't matter, thus we try to find a
      // matching polygon in the target mesh for each polygon in source mesh.
      maxon::BaseArray<bool> matchedPolys;
      iferr (matchedPolys.Resize(fcnt)) return false;

      // Helper function to compare two polygons.
      auto eq = [](CPolygon const& a, CPolygon const& b) -> Bool {
        for (Int32 i = 0; i < 4; ++i) {
          if (a[i] != b[i]) return false;
        }
        return true;
      };

      for (Int32 i = 0; i < fcnt; ++i) {
        // Find a matching polygon in target mesh.
        CPolygon const& a = polys[i];
        bool matched = false;
        for (Int32 j = 0; j < fcnt; ++j) {
          if (matchedPolys[j]) continue;
          CPolygon const& b = otherPolys[j];
          if (eq(a, b)) {
            matchedPolys[j] = true;
            matched = true;
            break;
          }
        }
        if (!matched) return false;
      }
    }

    return true;
  }

  //==========================================================================
  //==========================================================================
  BaseObject* CreateInstance(BaseDocument* doc, bool undos, ObjectInfo& source, Matrix& trm) {
    BaseObject* instance = BaseObject::Alloc(Oinstance);
    if (!instance) return nullptr;

    if (!source.isTarget) {
      Matrix off = utils::CenterAxis(source.op);
      source.op->SetMl(source.op->GetMl() * off);

      // Find the new transformation matrix after the source object is centered.
      Vector const* points = this->op->GetPointR();
      Vector const* otherPoints = source.op->GetPointR();
      Int32 const pcnt = this->op->GetPointCount();
      CriticalAssert(pcnt == source.op->GetPointCount());
      CriticalAssert(FindTransformationMatrix(points, otherPoints, pcnt, &trm));
    }

    instance->SetParameter(INSTANCEOBJECT_LINK, source.op, DESCFLAGS_SET_0);
    #if API_VERSION < 20000
      instance->SetParameter(INSTANCEOBJECT_RENDERINSTANCE, true, DESCFLAGS_SET_0);
    #else
      instance->SetParameter(INSTANCEOBJECT_RENDERINSTANCE_MODE, INSTANCEOBJECT_RENDERINSTANCE_MODE, DESCFLAGS_SET_0);
    #endif

    ObjectColorProperties cprops; this->op->GetColorProperties(&cprops);
    instance->SetColorProperties(&cprops);
    instance->SetEditorMode(this->op->GetEditorMode());
    instance->SetRenderMode(this->op->GetRenderMode());
    instance->SetLayerObject(this->op->GetLayerObject(doc));
    instance->SetName(this->op->GetName());
    instance->SetMl(this->op->GetMl());

    // Move all child objects and tags to the instance.
    for (BaseTag* tag = this->op->GetFirstTag(); tag; tag = tag->GetNext()) {
      if (tag->GetType() == Tphong || tag->IsInstanceOf(Tvariable)) continue;
      if (undos) doc->AddUndo(UNDOTYPE_CHANGE, tag);
      tag->Remove();
      instance->InsertTag(tag);
    }
    for (BaseObject* child = this->op->GetDown(); child; child = child->GetNext()) {
      if (undos) doc->AddUndo(UNDOTYPE_CHANGE, child);
      child->Remove();
      child->InsertUnderLast(instance);
    }

    // Create a Phong tag with the correct angle.
    if (!NearZero(this->phongAngle)) {
      BaseTag* tag = this->op->MakeTag(Tphong);
      if (tag && !NearZero(this->phongAngle - PI)) { // is not 180 deg
        tag->SetParameter(PHONGTAG_PHONG_ANGLELIMIT, false, DESCFLAGS_SET_0);
        tag->SetParameter(PHONGTAG_PHONG_ANGLE, this->phongAngle, DESCFLAGS_SET_0);
      }
    }

    source.isTarget = true;
    this->link = &source;
    return instance;
  }
};

//============================================================================
//============================================================================
class ResolveDuplicatesCommand : public CommandData {
public:

  // CommandData Overrides

  //==========================================================================
  //==========================================================================
  virtual Bool Execute(BaseDocument* doc) override;

};

//============================================================================
//============================================================================
Bool ResolveDuplicatesCommand::Execute(BaseDocument* doc) {
  // Ask the user if undos should be created. He/she should choose "No"
  // for large scenes as Cinema seems to have problems with large undo steps.
  Bool cancel = false;
  Bool const undos = ([&]() {
    String msg = GeLoadString(IDS_RESOLVEDUPLICATES_ASKUNDO);
    GEMB_R res = GeOutString(msg, GEMB_YESNOCANCEL);
    if (res == GEMB_R_CANCEL) cancel = true;
    return res == GEMB_R_YES;
  })();
  if (cancel)
    return true;

  if (undos) print::info("User chose to create an undo step for this operation.");
  else print::info("User chose to create NO undo step for this operation.");

  StatusSetSpin();
  StatusSetText(GeLoadString(IDS_RESOLVEDUPLICATES_COLLECTINFO));

  Float64 tstart = GeGetMilliSeconds();
  if (undos) doc->StartUndo();

  // Generate a list of #ObjectInfo structures of the while scene.
  maxon::BlockArray<ObjectInfo> objects;
  {
    BaseObject* op = doc->GetFirstObject();
    while (op) {
      if (op->IsInstanceOf(Opoint))
        objects.Append(ObjectInfo(ToPoint(op)));
      op = utils::GetNextNode(op);
    }
  }

  // Find duplicate objects and collect them in a list as well.
  StatusSetText(GeLoadString(IDS_RESOLVEDUPLICATES_FINDDUPLICATES));
  Int32 duplicateCount = 0;
  Matrix trm;
  for (ObjectInfo& info : objects) {
    if (info.isTarget) continue;
    for (ObjectInfo& other : objects) {
      if (info == other || other.link) continue;
      if (!info.Equals(other, trm)) continue;

      // Create an instance object from the other object that we matched.
      BaseObject* instance = info.CreateInstance(doc, undos, other, trm);
      if (!instance) continue;

      instance->SetMl(instance->GetMl() * ~trm);
      instance->InsertAfter(info.op);
      if (undos) doc->AddUndo(UNDOTYPE_NEW, instance);

      // Delete the original object.
      if (undos) doc->AddUndo(UNDOTYPE_DELETE, info.op);
      BaseObject::Free((BaseObject*&) info.op);
      ++duplicateCount;
      break;
    }
  }

  // Now also replace all source objects by instances. We need to do
  // this to rid the source object of all texture tags so that they
  // can be inherited from the instance object.
  BaseObject* root = BaseObject::Alloc(Onull);
  Int32 targetCount = 0;
  if (root) {
    for (ObjectInfo& info : objects) {
      if (!info.isTarget) continue;
      BaseObject* instance = info.CreateInstance(doc, undos, info, trm);
      if (!instance) continue;
      instance->InsertAfter(info.op);
      if (undos) {
        doc->AddUndo(UNDOTYPE_NEW, instance);
        doc->AddUndo(UNDOTYPE_CHANGE, info.op);
      }
      info.op->SetEditorMode(MODE_UNDEF);
      info.op->SetRenderMode(MODE_UNDEF);
      info.op->Remove();
      info.op->InsertUnder(root);
      ++targetCount;
    }
    if (targetCount > 0) {
      root->SetEditorMode(MODE_OFF);
      root->SetRenderMode(MODE_OFF);
      root->SetName("Instance Sources"_s);
      doc->InsertObject(root, nullptr, nullptr);
      if (undos) doc->AddUndo(UNDOTYPE_NEW, root);
    }
    else {
      BaseObject::Free(root);
    }
  }

  // Finalize.
  if (undos) doc->EndUndo();
  EventAdd();
  StatusClear();

  print::info("Resolved", duplicateCount, "duplicates from", targetCount,
    "sources in", (GeGetMilliSeconds() - tstart) / 1000.0, "seconds.");
  return true;
}

} // namespace resolve_duplicates

//============================================================================
//============================================================================
Bool RegisterResolveDuplicates() {
  menu::root().AddPlugin(PLUGIN_ID);
  using resolve_duplicates::ResolveDuplicatesCommand;
  String const name = GeLoadString(IDS_RESOLVEDUPLICATES_NAME);
  Int32 const info = PLUGINFLAG_HIDEPLUGINMENU;
  raii::AutoBitmap const icon("icons/resolve_duplicates.png");
  String const help = GeLoadString(IDS_RESOLVEDUPLICATES_HELP);
  return RegisterCommandPlugin(
    PLUGIN_ID,
    name,
    info,
    icon,
    help,
    NewObjClear(ResolveDuplicatesCommand));
}
