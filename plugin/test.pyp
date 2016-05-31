"""Offset-Y Spline
Takes a child-spline as input and offset all its points on the y-axis by a specific value. Tangents are unaffected.

Usage Instructions
------------------
1. Save in a file called OffsetYSpline.pyp
2. Locate it in the plugin folder and create the resources needed to have the offset value displayed as a parameter
   with ID 1000 in the Attribute Manager
3. Start Cinema
4. Create a generating spline
5. From the Plugin menu, select OffsetYSpline
6. Set the generating spline as input child of the OffsetySpline
"""


# =====================================================================================================================#
#   Imports
# =====================================================================================================================#
import c4d


# =====================================================================================================================#
#   Class Definitions
# =====================================================================================================================#
def OffsetSpline(childSpline, offsetValue):
    # retrieve child points count and type
    pointCnt = childSpline.GetPointCount()
    splineType = childSpline.GetInterpolationType()

    # allocate the result
    result = c4d.SplineObject(pointCnt, splineType)

    if result is None:
        return None

    # set the points position and tangency data
    for i in range(pointCnt):
        currPos = childSpline.GetPoint(i)
        #modify the y-value to make some change to the resulting spline
        result.SetPoint(i, c4d.Vector(currPos.x,currPos.y+offsetValue, currPos.z))
        result.SetTangent(i, childSpline.GetTangent(i)["vl"], childSpline.GetTangent(i)["vr"])

    # transfer the matrix information
    result.SetMl(childSpline.GetMl())

    # retrieve the spline object
    childRealSpline = childSpline.GetRealSpline()

    if childRealSpline:
        #retrieve the real spline BaseContainer and set the result spline parameters accordingly to those found in
        childRealSplineBC = childRealSpline.GetDataInstance()
        result.SetParameter(c4d.SPLINEOBJECT_INTERPOLATION, childRealSplineBC.GetInt32(c4d.SPLINEOBJECT_INTERPOLATION), c4d.DESCFLAGS_SET_FORCESET)
        result.SetParameter(c4d.SPLINEOBJECT_MAXIMUMLENGTH, childRealSplineBC.GetFloat(c4d.SPLINEOBJECT_MAXIMUMLENGTH), c4d.DESCFLAGS_SET_FORCESET)
        result.SetParameter(c4d.SPLINEOBJECT_SUB, childRealSplineBC.GetInt32(c4d.SPLINEOBJECT_SUB), c4d.DESCFLAGS_SET_FORCESET)
        result.SetParameter(c4d.SPLINEOBJECT_ANGLE, childRealSplineBC.GetFloat(c4d.SPLINEOBJECT_ANGLE), c4d.DESCFLAGS_SET_FORCESET)

    # return the computed spline
    return result


class OffsetYSpline(c4d.plugins.ObjectData):
    PLUGIN_ID = 1038342  # TODO: Replace with your own ID from PluginCafe.com

    def __init__(self):
        self.countourChildDirty = 0


    def Init(self, op):
        bc = op.GetDataInstance()
        if bc is None:
            return False

        bc.SetInt32(1000, 100)

        return True

    def CheckDirty(self, op, doc):
        # override the CheckDirty to use it on GetCountour
        childDirty = 0

        child = op.GetDown()

        if child:
            childDirty = child.GetDirty(c4d.DIRTYFLAGS_DATA | c4d.DIRTYFLAGS_MATRIX)

        if (childDirty != self.countourChildDirty):
            op.SetDirty(c4d.DIRTYFLAGS_DATA)

        self.countourChildDirty = childDirty;


    def GetVirtualObject(self, op, hh):
        # check the passed parameters
        if op is None or hh is None:
            return None

        # check the cache and allocate other data variables
        cache = op.GetCache()
        child = None
        childSpline = None
        sop = None

        # retrieve the current document
        doc = None
        if hh is None:
            doc = op.GetDocument
        else:
            doc = hh.GetDocument

        # retrieve the generator BaseContainer
        bc = op.GetDataInstance()
        offsetValue = 0
        if bc:
            offsetValue = bc.GetInt32(1000)

        # define dirty variables
        dirty = False
        cloneDirty = False


        child = op.GetDown()

        #
        if child:
            # check the dirtyness of the child
            temp = op.GetHierarchyClone(hh, child, c4d.HIERARCHYCLONEFLAGS_ASSPLINE, cloneDirty, trans)

            # if it's dirty then assign temp the object returned from GetHierarchyClone
            if temp is None:
                temp = op.GetHierarchyClone(hh, child, c4d.HIERARCHYCLONEFLAGS_ASSPLINE, None, trans)

            # sum up the clone flag
            dirty |= cloneDirty

            # assign temp object to childSpline
            # note: in C++ this assignment should explicitly cast from BaseObject to SplineObject since
            # childSpline is a SplineObject
            if (childSpline is None and temp and (temp.IsInstanceOf(c4d.Ospline) or (temp.GetInfo()&c4d.OBJECT_ISSPLINE) or temp.IsInstanceOf(c4d.Oline))):
                childSpline = temp

        # Touch the child to flag
        child.Touch()

        # check the dirtyness of the data in the Attribute Manager
        dirty |= op.IsDirty(c4d.DIRTYFLAGS_DATA)

        # return the cache data if not dirty and cache exists
        if (not dirty and cache):
            return cache

        # check for childSpline
        if (childSpline is None):
            return None

        # compute the resulting spline using a generic function - in this case a simple one changing the value of the points on y-axis
        sop = OffsetSpline(childSpline, offsetValue)
        if sop is None:
            return None

        # copy tags from child to resulting spline
        childSpline.CopyTagsTo(sop, True, c4d.NOTOK, c4d.NOTOK)

        # advice about the update occurred
        sop.Message(c4d.MSG_UPDATE)

        return sop


    def GetContour(self, op, doc, lod, bt):
        # check passed parameters
        if op is None:
            return None

        # retrieve the current active document
        if doc is None:
            doc = op.GetDocument()

        if doc is None:
            doc = GetActiveDocument()

        if doc is None:
            return None

        # allocate other data variables
        child = None
        childSpline = None
        sop = None

        # retrieve the generator BaseContainer
        bc = op.GetDataInstance()
        offsetValue = 0
        if bc:
            offsetValue = bc.GetInt32(1000)

        child = op.GetDown()

        if child:
            temp = child

            # a basic emulation of the GetHierarchyClone in order to retrieve the splines as required
            if (temp and not temp.IsInstanceOf(c4d.Ospline) and not temp.IsInstanceOf(c4d.Oline) and ((temp.GetInfo()&c4d.OBJECT_ISSPLINE) or temp.GetType() == c4d.Oinstance)):
                temp = temp.GetClone(c4d.COPYFLAGS_NO_ANIMATION, None)

                if temp is None:
                    return None

            # assign temp object to childSpline
            # note: in C++ this assignment should explicitly cast from BaseObject to SplineObject since
            # childSpline is a SplineObject
            if (childSpline is None and temp and (temp.IsInstanceOf(c4d.Ospline) or (temp.GetInfo()&c4d.OBJECT_ISSPLINE) or temp.IsInstanceOf(c4d.Oline))):
                childSpline = temp

        if childSpline is None:
            return None

        # compute the resulting spline using a generic function - in this case a simple one changing the value of the points on y-axis
        sop = OffsetSpline(childSpline, offsetValue)
        if sop is None:
            return None

        # advice about the update occurred
        sop.Message(c4d.MSG_UPDATE)

        return sop

# =====================================================================================================================#
#   Plugin registration
# =====================================================================================================================#

if __name__ == "__main__":
    c4d.plugins.RegisterObjectPlugin(id=OffsetYSpline.PLUGIN_ID,
                                     str="OffsetYSpline",
                                     g=OffsetYSpline,
                                     description="Gvcsv",
                                     icon=None,
                                     info=c4d.OBJECT_GENERATOR | c4d.OBJECT_ISSPLINE | c4d.OBJECT_INPUT)