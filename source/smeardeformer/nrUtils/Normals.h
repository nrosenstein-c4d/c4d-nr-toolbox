/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * nrUtils/Normals.h
 *
 * created: 2013-07-18
 * updated: 2013-07-19
 */

#ifndef NR_UTILS_NORMALS_H
#define NR_UTILS_NORMALS_H

    #include <c4d.h>

    namespace nr {

    void ComputeFaceNormals(
            const Vector* vertices, const CPolygon* faces,
            Vector* normals, Int32 face_count);

    Vector* ComputeFaceNormals(
            const Vector* vertices, Int32 vertex_count,
            const CPolygon* faces,  Int32 face_count);

    Bool ComputeVertexNormals(
            const CPolygon* faces, const Vector* face_normals, Int32 face_count,
            Vector* vertex_normals, Int32 vertex_count);

    Bool ComputeVertexNormals(
            const Vector* vertices, Int32 vertex_count,
            const CPolygon* faces, Int32 face_count,
            Vector* vertex_normals);

    Vector* ComputeVertexNormals(
            const CPolygon* faces, const Vector* face_normals, Int32 face_count,
            Int32 vertex_count);

    Vector* ComputeVertexNormals(
            const Vector* vertices, Int32 vertex_count,
            const CPolygon* faces, Int32 face_count);

    } // namespace nr

#endif /* NR_UTILS_NORMALS_H */
