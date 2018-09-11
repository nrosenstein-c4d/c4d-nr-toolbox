/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * nrUtils/Normals.cpp
 *
 * created: 2013-07-18
 * updated: 2013-07-19
 */

#include "Normals.h"
#include "Memory.h"

namespace nr {

void ComputeFaceNormals(
        const Vector* vertices, const CPolygon* faces,
        Vector* normals, Int32 face_count) {
    for (Int32 i=0; i < face_count; i++) {
        const CPolygon& f = faces[i];
        normals[i] = Cross(vertices[f.b] - vertices[f.a], vertices[f.d] - vertices[f.a]);
    }
}

Vector* ComputeFaceNormals(
        const Vector* vertices, Int32 vertex_count,
        const CPolygon* faces,  Int32 face_count) {
    Vector* normals = memory::Alloc<Vector>(face_count);
    if (normals) {
        ComputeFaceNormals(vertices, faces, normals, face_count);
    }
    return normals;
}

Bool ComputeVertexNormals(
        const CPolygon* faces, const Vector* face_normals, Int32 face_count,
        Vector* normals, Int32 vertex_count) {
    Neighbor nb;
    if (!nb.Init(vertex_count, faces, face_count, nullptr)) {
        return false;
    }

    for (Int32 i=0; i < vertex_count; i++) {
        // Get an array of face indecies that are connected with the
        // point.
        Int32* face_indecies = nullptr;
        Int32 count = 0;
        nb.GetPointPolys(i, &face_indecies, &count);

        // Sum up the face normals.
        Vector& normal = normals[i] = Vector(0);
        for (Int32 i=0; i < count; i++) {
            normal += face_normals[face_indecies[i]];
        }

        if (count > 0) normal *= (1.0 / (Float) count);
    }

    return true;
}

Bool ComputeVertexNormals(
        const Vector* vertices, Int32 vertex_count,
        const CPolygon* faces, Int32 face_count,
        Vector* normals) {
    Vector* face_normals = ComputeFaceNormals(vertices, vertex_count, faces, face_count);
    if (!face_normals) return false;
    Bool success = ComputeVertexNormals(faces, face_normals, face_count, normals, vertex_count);
    memory::Free(face_normals);
    return success;
}


Vector* ComputeVertexNormals(
        const CPolygon* faces, const Vector* face_normals, Int32 face_count,
        Int32 vertex_count) {
    Vector* normals = memory::Alloc<Vector>(vertex_count);
    if (normals) {
        if (!ComputeVertexNormals(faces, face_normals, face_count, normals, vertex_count)) {
            DeleteMem(normals);
            normals = nullptr;
        }
    }
    return normals;
}

Vector* ComputeVertexNormals(
        const Vector* vertices, Int32 vertex_count,
        const CPolygon* faces, Int32 face_count) {
    Vector* face_normals = ComputeFaceNormals(vertices, vertex_count, faces, face_count);
    Vector* vertex_normals = nullptr;
    if (face_normals) {
        vertex_normals = ComputeVertexNormals(faces, face_normals, face_count, vertex_count);
        DeleteMem(face_normals);
    }
    return face_normals;
}

} // namespace nr


