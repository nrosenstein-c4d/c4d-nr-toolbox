/**
 * Copyright (C) 2013-2015 Niklas Rosenstein
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 */

#include "misc/legacy.h"
#include <c4d_quaternion.h>
#include <c4d_baseeffectordata.h>
#include <c4d_falloffdata.h>
#include "lib_csv.h"
#include "CSVEffector.h"

#include "res/c4d_symbols.h"
#include "res/description/Ocsveffector.h"
#include "misc/raii.h"
#include "menu.h"

#define CSVEFFECTOR_VERSION 1000

typedef c4d_misc::BaseArray<Real> FloatArray;

static Vector VectorInterpolate(const Vector& a, const Vector& b, Real weight);

static Matrix MatrixInterpolate(const Matrix& a, const Matrix& b, Real weight);

static Real RangeMap(Real value, Real minIn, Real maxIn, Real minOut, Real maxOut);

struct RowConfiguration {
    LONG angle_mode;
    struct Triple {
        LONG x, y, z;
    } pos, scale, rot;
};

struct RowOperationData {
    Real minstrength;
    Real maxstrength;
    const FloatArray* minv;
    const FloatArray* maxv;
};

class CSVEffectorData : public EffectorData {

    typedef EffectorData super;

public:

    static NodeData* Alloc() { return gNew(CSVEffectorData); }

    //| EffectorData Overrides

    virtual void ModifyPoints(BaseObject* op, BaseObject* gen, BaseDocument* doc,
                              EffectorDataStruct* data, MoData* md, BaseThread* thread);

    //| NodeData Overrides

    virtual Bool Init(GeListNode* node);

    virtual Bool GetDDescription(GeListNode* node, Description* description, DESCFLAGS_DESC& flags);

    virtual Bool Message(GeListNode* node, LONG typeId, void* pData);

private:

    void GetRowConfiguration(const BaseContainer* bc, RowConfiguration* config);

    Real GetRowCell(const RowOperationData& data, const FloatArray& row, LONG index, Real vDefault=0.0);

    void FillCycleParameter(BaseContainer* itemdesc);

    void UpdateTable(BaseObject* op, BaseDocument* doc=NULL, BaseContainer* bc=NULL, Bool force=FALSE);

    MMFloatCSVTable m_table;

};


void CSVEffectorData::ModifyPoints(
            BaseObject* op, BaseObject* gen, BaseDocument* doc,
            EffectorDataStruct* data, MoData* md, BaseThread* thread) {
    BaseContainer* bc = op->GetDataInstance();
    if (!bc) return;
    UpdateTable(op, doc, bc);

    LONG rowCount = m_table.GetRowCount();
    if (rowCount <= 0) return;

    // Retrieve the row-configuration.
    RowConfiguration config;
    GetRowConfiguration(bc, &config);

    // Obtain the number of clones in the MoData and allocate a Matrix
    // array of such.
    LONG cloneCount = md->GetCount();
    if (cloneCount <= 0) return;
    c4d_misc::BaseArray<Matrix> matrices;
    matrices.Resize(Min<LONG>(rowCount, cloneCount));

    // Retrieve the mulipliers from the effector parameters.
    Vector mulPos = bc->GetVector(CSVEFFECTOR_MULTIPLIER_POS);
    Vector mulScale = bc->GetVector(CSVEFFECTOR_MULTIPLIER_SCALE);
    Vector mulRot = bc->GetVector(CSVEFFECTOR_MULTIPLIER_ROT);
    RowOperationData rowOpData;
    rowOpData.minstrength = data->minstrength;
    rowOpData.maxstrength = data->maxstrength;
    rowOpData.minv = &m_table.GetMinValues();
    rowOpData.maxv = &m_table.GetMaxValues();

    // Modify the matrices based on the CSV data.
    for (LONG i=0; i < matrices.GetCount(); i++) {
        const FloatArray& row = m_table.GetRow(i % rowCount);
        Matrix& matrix = matrices[i];

        Real h = GetRowCell(rowOpData, row, config.rot.x) * mulRot.x;
        Real p = GetRowCell(rowOpData, row, config.rot.y) * mulRot.y;
        Real b = GetRowCell(rowOpData, row, config.rot.z) * mulRot.z;
        if (config.angle_mode == CSVEFFECTOR_ANGLEMODE_DEGREES) {
            h = Rad(h);
            p = Rad(p);
            b = Rad(b);
        }
        matrix = HPBToMatrix(Vector(h, p, b), ROTATIONORDER_DEFAULT);

        Real xScale = GetRowCell(rowOpData, row, config.scale.x, 1.0) * mulScale.x;
        Real yScale = GetRowCell(rowOpData, row, config.scale.y, 1.0) * mulScale.y;
        Real zScale = GetRowCell(rowOpData, row, config.scale.z, 1.0) * mulScale.z;
        matrix.v1 *= xScale; // Vector(xScale, 0, 0);
        matrix.v2 *= yScale; // Vector(0, yScale, 0);
        matrix.v3 *= zScale; // Vector(0, 0, zScale);

        matrix.off.x = GetRowCell(rowOpData, row, config.pos.x) * mulPos.x;
        matrix.off.y = GetRowCell(rowOpData, row, config.pos.y) * mulPos.y;
        matrix.off.z = GetRowCell(rowOpData, row, config.pos.z) * mulPos.z;
    }

    // Retrieve the MD Matrices and additional important information
    // before finally adjusting the clones' matrices.
    MDArray<Matrix> destMatrices = md->GetMatrixArray(MODATA_MATRIX);
    MDArray<LONG> flagArray = md->GetLongArray(MODATA_FLAGS);
    MDArray<Real> weightArray = md->GetRealArray(MODATA_WEIGHT);
    if (!destMatrices) {
        GePrint("> WARNING: No Matrix Array could be retrieved."); // DEBUG
        return;
    }

    // Find the MoGraph selection tag.
    BaseTag* moTag = NULL;
    String moTagName = bc->GetString(ID_MG_BASEEFFECTOR_SELECTION);
    if (moTagName.Content()) moTag = gen->GetFirstTag();
    while (moTag) {
        if (moTag->IsInstanceOf(Tmgselection) && moTag->GetName() == moTagName) {
            break;
        }
        moTag = moTag->GetNext();
    }

    // Retrieve the BaseSelect from the MoGraph selection tag.
    BaseSelect* moSelection = NULL;
    if (moTag && moTag->IsInstanceOf(Tmgselection)) {
        GetMGSelectionMessage data;
        moTag->Message(MSG_GET_MODATASELECTION, &data);
        moSelection = data.sel;
    }

    C4D_Falloff* falloff = NULL;
    if (GetEffectorFlags() & EFFECTORFLAGS_HASFALLOFF) {
        falloff = GetFalloff();
        if (falloff && !falloff->InitFalloff(bc, doc, op)) falloff = NULL;
    }
    const Matrix mParent = gen->GetMg();

    // And figure how to iterate over the MD Matrices.
    LONG offset = bc->GetLong(CSVEFFECTOR_OFFSET);
    Bool repeat = bc->GetBool(CSVEFFECTOR_REPEAT);
    LONG start = 0, end = cloneCount;
    if (!repeat) {
        start = offset;
        end = offset + rowCount;
        if (end > cloneCount) end = cloneCount;
        if (start >= cloneCount) return;
        offset = 0;
    }

    Real weight = 0.0;

    // Now apply the changes.
    for (LONG i=start; i < end; i++) {
        // Don't calculate the particle if not necessary.
        if (moSelection && !moSelection->IsSelected(i)) continue;
        if (flagArray) {
            LONG flag = flagArray[i];
            if (!(flag & MOGENFLAG_CLONE_ON) || (flag & MOGENFLAG_DISABLE)) {
                continue;
            }
        }

        Matrix mOffset = matrices[(i - start) % matrices.GetCount()];
        Matrix& mDest  = destMatrices[(i + offset) % cloneCount];

        // Sample the weighting of the current particle.
        if (falloff) {
            Real moWeight = weightArray ? weightArray[i] : 1.0;
            falloff->Sample((mDest * mParent).off, &weight, TRUE, moWeight);
        }

        // The offset matrix is relative to the particles matrix. Make it absolute.
        /*mOffset.off += mDest.off;

        Matrix mDestNoOff = mDest;
        mDestNoOff.off = Vector();
        mOffset = mOffset * mDestNoOff;
        */
        mOffset = mDest * mOffset;

        // Now interpolate between the two states.
        mDest = MatrixInterpolate(mDest, mOffset, weight * data->strength);
    }
}

Bool CSVEffectorData::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return FALSE;
    BaseContainer* bc = ((BaseList2D*) node)->GetDataInstance();
    if (!bc) return FALSE;

    bc->SetFilename(CSVEFFECTOR_FILENAME, Filename(""));
    bc->SetString(CSVEFFECTOR_STATS, "");
    bc->SetBool(CSVEFFECTOR_HASHEADER, TRUE);
    bc->SetLong(CSVEFFECTOR_OFFSET, 0);

    bc->SetBool(CSVEFFECTOR_REPEAT, TRUE);
    bc->SetLong(CSVEFFECTOR_DELIMITER, CSVEFFECTOR_DELIMITER_COMMA);
    bc->SetLong(CSVEFFECTOR_ANGLEMODE, CSVEFFECTOR_ANGLEMODE_DEGREES);

    bc->SetLong(CSVEFFECTOR_ASSIGNMENT_XPOS, -1);
    bc->SetLong(CSVEFFECTOR_ASSIGNMENT_YPOS, -1);
    bc->SetLong(CSVEFFECTOR_ASSIGNMENT_ZPOS, -1);

    bc->SetLong(CSVEFFECTOR_ASSIGNMENT_XSCALE, -1);
    bc->SetLong(CSVEFFECTOR_ASSIGNMENT_YSCALE, -1);
    bc->SetLong(CSVEFFECTOR_ASSIGNMENT_ZSCALE, -1);

    bc->SetLong(CSVEFFECTOR_ASSIGNMENT_XROT, -1);
    bc->SetLong(CSVEFFECTOR_ASSIGNMENT_YROT, -1);
    bc->SetLong(CSVEFFECTOR_ASSIGNMENT_ZROT, -1);

    bc->SetVector(CSVEFFECTOR_MULTIPLIER_POS, Vector(1));
    bc->SetVector(CSVEFFECTOR_MULTIPLIER_SCALE, Vector(1));
    bc->SetVector(CSVEFFECTOR_MULTIPLIER_ROT, Vector(1));
    return TRUE;
}

Bool CSVEffectorData::GetDDescription(
            GeListNode* node, Description* desc, DESCFLAGS_DESC& flags) {
    if (!node || !super::GetDDescription(node, desc, flags)) return FALSE;

    // Fill in the cycle-fields of the assignment parameters.
    AutoAlloc<AtomArray> arr;
    FillCycleParameter(desc->GetParameterI(CSVEFFECTOR_ASSIGNMENT_XPOS, arr));
    FillCycleParameter(desc->GetParameterI(CSVEFFECTOR_ASSIGNMENT_YPOS, arr));
    FillCycleParameter(desc->GetParameterI(CSVEFFECTOR_ASSIGNMENT_ZPOS, arr));
    FillCycleParameter(desc->GetParameterI(CSVEFFECTOR_ASSIGNMENT_XSCALE, arr));
    FillCycleParameter(desc->GetParameterI(CSVEFFECTOR_ASSIGNMENT_YSCALE, arr));
    FillCycleParameter(desc->GetParameterI(CSVEFFECTOR_ASSIGNMENT_ZSCALE, arr));
    FillCycleParameter(desc->GetParameterI(CSVEFFECTOR_ASSIGNMENT_XROT, arr));
    FillCycleParameter(desc->GetParameterI(CSVEFFECTOR_ASSIGNMENT_YROT, arr));
    FillCycleParameter(desc->GetParameterI(CSVEFFECTOR_ASSIGNMENT_ZROT, arr));

    return TRUE;
}

Bool CSVEffectorData::Message(GeListNode* node, LONG type, void* pData) {
    if (!node) return FALSE;
    switch (type) {
    case MSG_DESCRIPTION_COMMAND: {
        DescriptionCommand* data = (DescriptionCommand*) pData;
        if (!data) return FALSE;
        if (data->id == CSVEFFECTOR_FORCERELOAD) {
            UpdateTable((BaseObject*) node, NULL, NULL, TRUE);
        }
        break;
    }
    case MSG_DESCRIPTION_POSTSETPARAMETER: {
        DescriptionPostSetValue* data = (DescriptionPostSetValue*) pData;
        if (!data) return FALSE;
        LONG id = (*data->descid)[data->descid->GetDepth() - 1].id;

        switch (id) {
        case CSVEFFECTOR_HASHEADER:
        case CSVEFFECTOR_FILENAME:
        case CSVEFFECTOR_DELIMITER:
            UpdateTable((BaseObject*) node, NULL, NULL, TRUE);
            break;
        default:
            break;
        } // end switch

        break;
    }
    } // end switch

    return super::Message(node, type, pData);
}

void CSVEffectorData::GetRowConfiguration(const BaseContainer* bc, RowConfiguration* config) {
    config->pos.x = bc->GetLong(CSVEFFECTOR_ASSIGNMENT_XPOS);
    config->pos.y = bc->GetLong(CSVEFFECTOR_ASSIGNMENT_YPOS);
    config->pos.z = bc->GetLong(CSVEFFECTOR_ASSIGNMENT_ZPOS);
    config->scale.x = bc->GetLong(CSVEFFECTOR_ASSIGNMENT_XSCALE);
    config->scale.y = bc->GetLong(CSVEFFECTOR_ASSIGNMENT_YSCALE);
    config->scale.z = bc->GetLong(CSVEFFECTOR_ASSIGNMENT_ZSCALE);
    config->rot.x = bc->GetLong(CSVEFFECTOR_ASSIGNMENT_XROT);
    config->rot.y = bc->GetLong(CSVEFFECTOR_ASSIGNMENT_YROT);
    config->rot.z = bc->GetLong(CSVEFFECTOR_ASSIGNMENT_ZROT);
    config->angle_mode = bc->GetLong(CSVEFFECTOR_ANGLEMODE);
}

Real CSVEffectorData::GetRowCell(const RowOperationData& data, const FloatArray& row, LONG index, Real vDefault) {
    if (index < 0 || index >= row.GetCount()) return vDefault;
    Real value = row[index];
    Real minv = (*data.minv)[index];
    Real maxv = (*data.maxv)[index];
    Real x = RangeMap(value, minv, maxv, data.minstrength, data.maxstrength);
    return value * x;
}

void CSVEffectorData::FillCycleParameter(BaseContainer* itemdesc) {
    if (!itemdesc) return;
    const BaseContainer& ref = m_table.GetHeaderContainer();
    itemdesc->SetContainer(DESC_CYCLE, ref);
}

void CSVEffectorData::UpdateTable(BaseObject* op, BaseDocument* doc, BaseContainer* bc, Bool force) {
    if (!bc) bc = op->GetDataInstance();
    if (!bc) return;
    if (!doc) doc = op->GetDocument();

    // Retrieve the delimiter and validate it.
    LONG delimiter = bc->GetLong(CSVEFFECTOR_DELIMITER);
    if (delimiter < 0 || delimiter > 255) {
        delimiter = CSVEFFECTOR_DELIMITER_COMMA;
    }
    m_table.SetDelimiter((CHAR) delimiter);
    m_table.SetHasHeader(bc->GetBool(CSVEFFECTOR_HASHEADER));

    // Retrieve the filename and make it relative if it does not
    // exist the way it was retrieved.
    Filename filename = bc->GetFilename(CSVEFFECTOR_FILENAME);
    if (doc && !GeFExist(filename)) {
        filename = doc->GetDocumentPath() + filename;
    }

    Bool updated = FALSE;
    Bool success = m_table.Init(filename, force, &updated);
    if (updated) {
        // The description must be reloaded if the table did update.
        op->SetDirty(DIRTYFLAGS_DESCRIPTION);

        // Update the statistics information in the effector parameters.
        String stats;
        if (m_table.Loaded()) {
            String rowCnt = LongToString(m_table.GetRowCount());
            String colCnt = LongToString(m_table.GetColumnCount());
            stats = GeLoadString(IDC_CSVEFFECTOR_STATS_FORMAT, rowCnt, colCnt);
        }
        else if (success) {
            stats = GeLoadString(IDC_CSVEFFECTOR_STATS_NOFILE);
        }
        else {
            stats = GeLoadString(IDC_CSVEFFECTOR_STATS_UNKOWNERROR);
        }
        bc->SetString(CSVEFFECTOR_STATS, stats);
    }
    else if (!success) {
        GePrint("No success initializing CSV file: " + LongToString(m_table.GetLastError()));
    }
}

//| Functions
//| ==========================================================================

Vector VectorInterpolate(const Vector& a, const Vector& b, Real weight) {
    return a + (b - a) * weight;
}

Real VectorLengthInterpolate(const Vector& a, const Vector& b, Real weight) {
    Real base = a.GetSquaredLength();
    return Sqrt(base + (b.GetSquaredLength() - base) * weight);
}

Matrix MatrixInterpolate(const Matrix& a, const Matrix& b, Real weight) {
    Matrix mA = a; mA.Normalize();
    Matrix mB = b; mB.Normalize();
    Quaternion qA, qB;
    qA.SetMatrix(mA);
    qB.SetMatrix(mB);

    Matrix m = QSlerp(qA, qB, weight).GetMatrix();
    m.off = VectorInterpolate(a.off, b.off, weight);
    m.v1 *= VectorLengthInterpolate(a.v1, b.v1, weight);
    m.v2 *= VectorLengthInterpolate(a.v2, b.v2, weight);
    m.v3 *= VectorLengthInterpolate(a.v3, b.v3, weight);
    return m;
}

Real RangeMap(Real value, Real minIn, Real maxIn, Real minOut, Real maxOut) {
    return ((value - minIn) / (maxIn - minIn)) * (maxOut - minOut) + minOut;
}


Bool RegisterCSVEffector() {
    menu::root().AddPlugin(IDS_MENU_EFFECTORS, Ocsveffector);
    return RegisterEffectorPlugin(
            Ocsveffector,
            GeLoadString(IDC_CSVEFFECTOR_NAME),
            OBJECT_MODIFIER | PLUGINFLAG_HIDEPLUGINMENU | OBJECT_CALL_ADDEXECUTION,
            CSVEffectorData::Alloc,
            "Ocsveffector",
            raii::AutoBitmap("icons/Ocsveffector.tif"),
            CSVEFFECTOR_VERSION);
}




