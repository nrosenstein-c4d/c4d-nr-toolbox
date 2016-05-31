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

    #include "misc/legacy.h"

    namespace nr {

    void ComputeFaceNormals(
            const Vector* vertices, const CPolygon* faces,
            Vector* normals, LONG face_count);

    Vector* ComputeFaceNormals(
            const Vector* vertices, LONG vertex_count,
            const CPolygon* faces,  LONG face_count);

    Bool ComputeVertexNormals(
            const CPolygon* faces, const Vector* face_normals, LONG face_count,
            Vector* vertex_normals, LONG vertex_count);

    Bool ComputeVertexNormals(
            const Vector* vertices, LONG vertex_count,
            const CPolygon* faces, LONG face_count,
            Vector* vertex_normals);

    Vector* ComputeVertexNormals(
            const CPolygon* faces, const Vector* face_normals, LONG face_count,
            LONG vertex_count);

    Vector* ComputeVertexNormals(
            const Vector* vertices, LONG vertex_count,
            const CPolygon* faces, LONG face_count);

    } // namespace nr

#endif /* NR_UTILS_NORMALS_H */
