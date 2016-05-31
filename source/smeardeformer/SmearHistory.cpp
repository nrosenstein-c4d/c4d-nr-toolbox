/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * SmearHistory.cpp
 */

#include "SmearHistory.h"

#include "nrUtils/Memory.h"
#include "nrUtils/Normals.h"

using namespace nr::memory;

Bool SmearState::Resize(LONG count) {
    Bool success = TRUE;
    success = success && Realloc<Vector>(original_vertices, count);
    success = success && Realloc<Vector>(original_normals, count);
    success = success && Realloc<Vector>(deformed_vertices, count);
    success = success && Realloc<Vector>(deformed_normals, count);
    success = success && Realloc<SReal>(weights, count);
    vertex_count = count;
    initialized = success;
    return success;
}

void SmearState::Flush() {
    Free(original_vertices);
    Free(original_normals);
    Free(deformed_vertices);
    Free(deformed_normals);
    vertex_count = 0;
    initialized = FALSE;
}


SmearSession* SmearHistory::NewSession(LONG max_history_count, Bool fake_session) {
    if (max_history_count < 1) max_history_count = 1;
    m_level_override = -1;
    SmearState state;
    SmearState* ptr = NULL;
    if (!fake_session || GetHistoryCount() <= 0) {
        fake_session = FALSE;
        // Remove all superfluos history elements. One item is implicit.
        while (m_states.GetCount() > max_history_count) {
            if (state.initialized) state.Flush();
            m_states.Pop(&state);
        }
        ptr = m_states.Insert(0, state);
    }
    else {
        m_level_override = max_history_count + 1;
        ptr = &m_states[0];
    }

    if (ptr) {
        return gNew(SmearSession, this, ptr, fake_session);
    }
    return NULL; // memory error
}

SmearHistory::~SmearHistory() {
    Reset();
}

void SmearHistory::FreeSession(SmearSession*& session) {
    // If the session was not updated (ie. all data is stored
    // successfully), we remove the state which has been created for
    // that sessions.
    if (!session->IsUpToDate()) {
        GePrint(String(__FUNCTION__) + ": Smear state is not up to date.");
        m_states.Pop(0);
    }
    GeFree(session);
    session = NULL;
}

LONG SmearHistory::GetHistoryCount() const {
    LONG count = m_states.GetCount();
    if (m_level_override > 0 && count > m_level_override) {
        count = m_level_override + 1;
    }
    return count;
}

const SmearState* SmearHistory::GetState(LONG index) const {
    if (index < 0 || index >= GetHistoryCount()) {
        return NULL;
    }
    if (m_level_override > 0 && index >= m_level_override) {
        return NULL;
    }
    if (!m_enabled && index != 0) {
        return NULL;
    }
    return &m_states[index];
}

void SmearHistory::Reset() {
    StateArray::Iterator it = m_states.Begin();
    for (; it != m_states.End(); it++) {
        it->Flush();
    }
    m_states.Flush();
}


SmearSession::SmearSession(SmearHistory* history, SmearState* state, Bool fake)
: m_state(state), m_created(FALSE), m_updated(FALSE),
  m_vertices(NULL), m_vertex_count(0), m_faces(NULL), m_face_count(0),
  m_fake(fake) {
}

SmearSession::~SmearSession() {
}

Bool SmearSession::CreateState(
        const Matrix& mg,
        const Vector* vertices, LONG vertex_count,
        const CPolygon* faces, LONG face_count) {
    if (m_created) {
        return FALSE;
    }

    m_vertices = vertices;
    m_vertex_count = vertex_count;
    m_faces = faces;
    m_face_count = face_count;

    if (!m_fake) {
        m_state->mg = mg;
        if (!m_state->Resize(vertex_count)) {
            return FALSE;
        }
        // Copy the vertices to the storage in the SmearState.
        for (LONG i=0; i < vertex_count; i++) {
            m_state->original_vertices[i] = m_state->mg * vertices[i];
        }

        // And compute the vertex normals. These are independent from
        // the global position of the vertices as long as their relations
        // are the same.
        if (!nr::ComputeVertexNormals(
                m_vertices, m_vertex_count, m_faces,
                m_face_count, m_state->original_normals)) {
            return FALSE;
        }
    }

    m_created = TRUE;
    return TRUE;
}

Bool SmearSession::DeformationComplete(const Matrix& mg) {
    if (!m_created || m_updated) {
        GePrint(String(__FUNCTION__) + ": Not yet created or already updated.");
        return FALSE;
    }

    // m_state->mg = mg;

    // Copy the vertices to the deformed storage in the SmearState
    // using the pointers passed to `CreateState()`.
    for (LONG i=0; i < m_vertex_count; i++) {
        m_state->deformed_vertices[i] = /* m_state-> */ mg * m_vertices[i];
    }

    // And calculate the vertex normals of the deformed points.
    if (!nr::ComputeVertexNormals(
            m_vertices, m_vertex_count, m_faces,
            m_face_count, m_state->deformed_normals)) {
        GePrint(String(__FUNCTION__) + ": Vertex normals not calculated.");
        return FALSE;
    }
    m_updated = TRUE;
    return TRUE;
}





