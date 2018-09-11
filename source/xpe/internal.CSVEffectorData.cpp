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

#include <c4d_apibridge.h>
#include <c4d_quaternion.h>
#include <c4d_baseeffectordata.h>
#include <c4d_falloffdata.h>
#include "lib_csv.h"
#include "CSVEffector.h"

#include "res/c4d_symbols.h"
#include "res/description/Ocsveffector.h"
#include "misc/raii.h"
#include "menu.h"

using namespace c4d_apibridge::M;

#define CSVEFFECTOR_VERSION 1000

typedef maxon::BaseArray<Float> FloatArray;

static Vector VectorInterpolate(const Vector& a, const Vector& b, Float weight);

static Matrix MatrixInterpolate(const Matrix& a, const Matrix& b, Float weight);

static Float RangeMap(Float value, Float minIn, Float maxIn, Float minOut, Float maxOut);

struct RowConfiguration {
    Int32 angle_mode;
    struct Triple {
        Int32 x, y, z;
    };
    Triple pos, scale, rot;
};

struct RowOperationData {
    Float minstrength;
    Float maxstrength;
    const FloatArray* minv;
    const FloatArray* maxv;
};

class CSVEffectorData : public EffectorData {

    typedef EffectorData super;

public:

    static NodeData* Alloc() { return NewObjClear(CSVEffectorData); }

    //| EffectorData Overrides

    virtual void ModifyPoints(BaseObject* op, BaseObject* gen, BaseDocument* doc,
                              EffectorDataStruct* data, MoData* md, BaseThread* thread);

    //| NodeData Overrides

    virtual Bool Init(GeListNode* node);

    virtual Bool GetDDescription(GeListNode* node, Description* description, DESCFLAGS_DESC& flags);

    virtual Bool Message(GeListNode* node, Int32 typeId, void* pData);

private:

    void GetRowConfiguration(const BaseContainer* bc, RowConfiguration* config);

    Float GetRowCell(const RowOperationData& data, const FloatArray& row, Int32 index, Float vDefault=0.0);

    void FillCycleParameter(BaseContainer* itemdesc);

    void UpdateTable(BaseObject* op, BaseDocument* doc=nullptr, BaseContainer* bc=nullptr, Bool force=false);

    MMFloatCSVTable m_table;

};


void CSVEffectorData::ModifyPoints(
            BaseObject* op, BaseObject* gen, BaseDocument* doc,
            EffectorDataStruct* data, MoData* md, BaseThread* thread) {
    BaseContainer* bc = op->GetDataInstance();
    if (!bc) return;
    UpdateTable(op, doc, bc);

    Int32 rowCount = m_table.GetRowCount();
    if (rowCount <= 0) return;

    // Retrieve the row-configuration.
    RowConfiguration config;
    GetRowConfiguration(bc, &config);

    // Obtain the number of clones in the MoData and allocate a Matrix
    // array of such.
    Int32 cloneCount = md->GetCount();
    if (cloneCount <= 0) return;
    maxon::BaseArray<Matrix> matrices;
    (void) matrices.Resize(Min<Int32>(rowCount, cloneCount));

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
    for (Int32 i=0; i < matrices.GetCount(); i++) {
        const FloatArray& row = m_table.GetRow(i % rowCount);
        Matrix& matrix = matrices[i];

        Float h = GetRowCell(rowOpData, row, config.rot.x) * mulRot.x;
        Float p = GetRowCell(rowOpData, row, config.rot.y) * mulRot.y;
        Float b = GetRowCell(rowOpData, row, config.rot.z) * mulRot.z;
        if (config.angle_mode == CSVEFFECTOR_ANGLEMODE_DEGREES) {
            h = Rad(h);
            p = Rad(p);
            b = Rad(b);
        }
        matrix = HPBToMatrix(Vector(h, p, b), ROTATIONORDER_DEFAULT);

        Float xScale = GetRowCell(rowOpData, row, config.scale.x, 1.0) * mulScale.x;
        Float yScale = GetRowCell(rowOpData, row, config.scale.y, 1.0) * mulScale.y;
        Float zScale = GetRowCell(rowOpData, row, config.scale.z, 1.0) * mulScale.z;
        Mv1(matrix) *= xScale; // Vector(xScale, 0, 0);
        Mv2(matrix) *= yScale; // Vector(0, yScale, 0);
        Mv3(matrix) *= zScale; // Vector(0, 0, zScale);

        Moff(matrix).x = GetRowCell(rowOpData, row, config.pos.x) * mulPos.x;
        Moff(matrix).y = GetRowCell(rowOpData, row, config.pos.y) * mulPos.y;
        Moff(matrix).z = GetRowCell(rowOpData, row, config.pos.z) * mulPos.z;
    }

    // Retrieve the MD Matrices and additional important information
    // before finally adjusting the clones' matrices.
    MDArray<Matrix> destMatrices = md->GetMatrixArray(MODATA_MATRIX);
    MDArray<Int32> flagArray = md->GetLongArray(MODATA_FLAGS);
    MDArray<Float> weightArray = md->GetRealArray(MODATA_WEIGHT);
    if (!destMatrices) {
        GePrint("> WARNING: No Matrix Array could be retrieved."); // DEBUG
        return;
    }

    // Find the MoGraph selection tag.
    BaseTag* moTag = nullptr;
    String moTagName = bc->GetString(ID_MG_BASEEFFECTOR_SELECTION);
    if (!c4d_apibridge::IsEmpty(moTagName)) moTag = gen->GetFirstTag();
    while (moTag) {
        if (moTag->IsInstanceOf(Tmgselection) && moTag->GetName() == moTagName) {
            break;
        }
        moTag = moTag->GetNext();
    }

    // Retrieve the BaseSelect from the MoGraph selection tag.
    BaseSelect* moSelection = nullptr;
    if (moTag && moTag->IsInstanceOf(Tmgselection)) {
        GetMGSelectionMessage data;
        moTag->Message(MSG_GET_MODATASELECTION, &data);
        moSelection = data.sel;
    }

    C4D_Falloff* falloff = nullptr;
    if (GetEffectorFlags() & EFFECTORFLAGS_HASFALLOFF) {
        falloff = GetFalloff();
        if (falloff && !falloff->InitFalloff(bc, doc, op)) falloff = nullptr;
    }
    const Matrix mParent = gen->GetMg();

    // And figure how to iterate over the MD Matrices.
    Int32 offset = bc->GetInt32(CSVEFFECTOR_OFFSET);
    Bool repeat = bc->GetBool(CSVEFFECTOR_REPEAT);
    Int32 start = 0, end = cloneCount;
    if (!repeat) {
        start = offset;
        end = offset + rowCount;
        if (end > cloneCount) end = cloneCount;
        if (start >= cloneCount) return;
        offset = 0;
    }

    Float weight = 0.0;

    // Now apply the changes.
    for (Int32 i=start; i < end; i++) {
        // Don't calculate the particle if not necessary.
        if (moSelection && !moSelection->IsSelected(i)) continue;
        if (flagArray) {
            Int32 flag = flagArray[i];
            if (!(flag & MOGENFLAG_CLONE_ON) || (flag & MOGENFLAG_DISABLE)) {
                continue;
            }
        }

        Matrix mOffset = matrices[(i - start) % matrices.GetCount()];
        Matrix& mDest  = destMatrices[(i + offset) % cloneCount];

        // Sample the weighting of the current particle.
        if (falloff) {
            Float moWeight = weightArray ? weightArray[i] : 1.0;
            falloff->Sample((mDest * mParent).off, &weight, true, moWeight);
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
    if (!node || !super::Init(node)) return false;
    BaseContainer* bc = ((BaseList2D*) node)->GetDataInstance();
    if (!bc) return false;

    bc->SetFilename(CSVEFFECTOR_FILENAME, Filename(""));
    bc->SetString(CSVEFFECTOR_STATS, ""_s);
    bc->SetBool(CSVEFFECTOR_HASHEADER, true);
    bc->SetInt32(CSVEFFECTOR_OFFSET, 0);

    bc->SetBool(CSVEFFECTOR_REPEAT, true);
    bc->SetInt32(CSVEFFECTOR_DELIMITER, CSVEFFECTOR_DELIMITER_COMMA);
    bc->SetInt32(CSVEFFECTOR_ANGLEMODE, CSVEFFECTOR_ANGLEMODE_DEGREES);

    bc->SetInt32(CSVEFFECTOR_ASSIGNMENT_XPOS, -1);
    bc->SetInt32(CSVEFFECTOR_ASSIGNMENT_YPOS, -1);
    bc->SetInt32(CSVEFFECTOR_ASSIGNMENT_ZPOS, -1);

    bc->SetInt32(CSVEFFECTOR_ASSIGNMENT_XSCALE, -1);
    bc->SetInt32(CSVEFFECTOR_ASSIGNMENT_YSCALE, -1);
    bc->SetInt32(CSVEFFECTOR_ASSIGNMENT_ZSCALE, -1);

    bc->SetInt32(CSVEFFECTOR_ASSIGNMENT_XROT, -1);
    bc->SetInt32(CSVEFFECTOR_ASSIGNMENT_YROT, -1);
    bc->SetInt32(CSVEFFECTOR_ASSIGNMENT_ZROT, -1);

    bc->SetVector(CSVEFFECTOR_MULTIPLIER_POS, Vector(1));
    bc->SetVector(CSVEFFECTOR_MULTIPLIER_SCALE, Vector(1));
    bc->SetVector(CSVEFFECTOR_MULTIPLIER_ROT, Vector(1));
    return true;
}

Bool CSVEffectorData::GetDDescription(
            GeListNode* node, Description* desc, DESCFLAGS_DESC& flags) {
    if (!node || !super::GetDDescription(node, desc, flags)) return false;

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

    return true;
}

Bool CSVEffectorData::Message(GeListNode* node, Int32 type, void* pData) {
    if (!node) return false;
    switch (type) {
    case MSG_DESCRIPTION_COMMAND: {
        DescriptionCommand* data = (DescriptionCommand*) pData;
        if (!data) return false;
        if (c4d_apibridge::GetDescriptionID(data) == CSVEFFECTOR_FORCERELOAD) {
            UpdateTable((BaseObject*) node, nullptr, nullptr, true);
        }
        break;
    }
    case MSG_DESCRIPTION_POSTSETPARAMETER: {
        DescriptionPostSetValue* data = (DescriptionPostSetValue*) pData;
        if (!data) return false;
        Int32 id = (*data->descid)[data->descid->GetDepth() - 1].id;

        switch (id) {
        case CSVEFFECTOR_HASHEADER:
        case CSVEFFECTOR_FILENAME:
        case CSVEFFECTOR_DELIMITER:
            UpdateTable((BaseObject*) node, nullptr, nullptr, true);
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
    config->pos.x = bc->GetInt32(CSVEFFECTOR_ASSIGNMENT_XPOS);
    config->pos.y = bc->GetInt32(CSVEFFECTOR_ASSIGNMENT_YPOS);
    config->pos.z = bc->GetInt32(CSVEFFECTOR_ASSIGNMENT_ZPOS);
    config->scale.x = bc->GetInt32(CSVEFFECTOR_ASSIGNMENT_XSCALE);
    config->scale.y = bc->GetInt32(CSVEFFECTOR_ASSIGNMENT_YSCALE);
    config->scale.z = bc->GetInt32(CSVEFFECTOR_ASSIGNMENT_ZSCALE);
    config->rot.x = bc->GetInt32(CSVEFFECTOR_ASSIGNMENT_XROT);
    config->rot.y = bc->GetInt32(CSVEFFECTOR_ASSIGNMENT_YROT);
    config->rot.z = bc->GetInt32(CSVEFFECTOR_ASSIGNMENT_ZROT);
    config->angle_mode = bc->GetInt32(CSVEFFECTOR_ANGLEMODE);
}

Float CSVEffectorData::GetRowCell(const RowOperationData& data, const FloatArray& row, Int32 index, Float vDefault) {
    if (index < 0 || index >= row.GetCount()) return vDefault;
    Float value = row[index];
    Float minv = (*data.minv)[index];
    Float maxv = (*data.maxv)[index];
    Float x = RangeMap(value, minv, maxv, data.minstrength, data.maxstrength);
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
    Int32 delimiter = bc->GetInt32(CSVEFFECTOR_DELIMITER);
    if (delimiter < 0 || delimiter > 255) {
        delimiter = CSVEFFECTOR_DELIMITER_COMMA;
    }
    m_table.SetDelimiter((Char) delimiter);
    m_table.SetHasHeader(bc->GetBool(CSVEFFECTOR_HASHEADER));

    // Retrieve the filename and make it relative if it does not
    // exist the way it was retrieved.
    Filename filename = bc->GetFilename(CSVEFFECTOR_FILENAME);
    if (doc && !GeFExist(filename)) {
        filename = doc->GetDocumentPath() + filename;
    }

    Bool updated = false;
    Bool success = m_table.Init(filename, force, &updated);
    if (updated) {
        // The description must be reloaded if the table did update.
        op->SetDirty(DIRTYFLAGS_DESCRIPTION);

        // Update the statistics information in the effector parameters.
        String stats;
        if (m_table.Loaded()) {
            String rowCnt = String::IntToString(m_table.GetRowCount());
            String colCnt = String::IntToString(m_table.GetColumnCount());
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
        GePrint("No success initializing CSV file: " + String::IntToString(m_table.GetLastError()));
    }
}

//| Functions
//| ==========================================================================

Vector VectorInterpolate(const Vector& a, const Vector& b, Float weight) {
    return a + (b - a) * weight;
}

Float VectorLengthInterpolate(const Vector& a, const Vector& b, Float weight) {
    Float base = a.GetSquaredLength();
    return Sqrt(base + (b.GetSquaredLength() - base) * weight);
}

Matrix MatrixInterpolate(const Matrix& a, const Matrix& b, Float weight) {
    Matrix mA = a; c4d_apibridge::Normalize(mA);
    Matrix mB = b; c4d_apibridge::Normalize(mB);
    Quaternion qA, qB;
    qA.SetMatrix(mA);
    qB.SetMatrix(mB);

    Matrix m = QSlerp(qA, qB, weight).GetMatrix();
    Moff(m) = VectorInterpolate(Moff(a), Moff(b), weight);
    Mv1(m) *= VectorLengthInterpolate(Mv1(a), Mv1(b), weight);
    Mv2(m) *= VectorLengthInterpolate(Mv2(a), Mv2(b), weight);
    Mv3(m) *= VectorLengthInterpolate(Mv3(a), Mv3(b), weight);
    return m;
}

Float RangeMap(Float value, Float minIn, Float maxIn, Float minOut, Float maxOut) {
    return ((value - minIn) / (maxIn - minIn)) * (maxOut - minOut) + minOut;
}


Bool RegisterCSVEffector() {
    menu::root().AddPlugin(IDS_MENU_EFFECTORS, Ocsveffector);
    return RegisterEffectorPlugin(
        Ocsveffector,
        GeLoadString(IDC_CSVEFFECTOR_NAME),
        OBJECT_MODIFIER | PLUGINFLAG_HIDEPLUGINMENU | OBJECT_CALL_ADDEXECUTION,
        CSVEffectorData::Alloc,
        "Ocsveffector"_s,
        raii::AutoBitmap("icons/Ocsveffector.tif"_s),
        CSVEFFECTOR_VERSION);
}
