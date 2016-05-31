/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * SmearHistory.h
 */

#ifndef NR_SMEARHISTORY_H
#define NR_SMEARHISTORY_H

    #include "misc/legacy.h"

    class SmearSession;

    struct SmearState {

        Vector* original_vertices;
        Vector* original_normals;
        Vector* deformed_vertices;
        Vector* deformed_normals;
        SReal*  weights; // filled by the smear deformer
        Matrix mg;
        LONG vertex_count;
        Bool initialized;

        SmearState()
        : original_vertices(NULL), original_normals(NULL),
          deformed_vertices(NULL), deformed_normals(NULL),
          weights(NULL), vertex_count(0), initialized(FALSE) { }

        ~SmearState() {
        }

        Bool Resize(LONG count);

        void Flush();

    };

    class SmearHistory {

        friend class SmearSession;

    public:

        SmearHistory()
        : m_enabled(TRUE), m_level_override(-1) { }

        ~SmearHistory();

        SmearSession* NewSession(LONG max_history_count, Bool fake_session=FALSE);

        void FreeSession(SmearSession*& session);

        LONG GetHistoryCount() const;

        const SmearState* GetState(LONG index) const;

        void SetEnabled(Bool enabled) { m_enabled = enabled; }

        void Reset();

        Bool CompareTime(const BaseTime& time) {
            if (m_prevtime != time) {
                m_prevtime = time;
                return TRUE;
            }
            return FALSE;
        }

        void CapHistoryLevel(LONG history_level) {
            if (history_level < 1) history_level = 1;
            m_level_override = history_level + 1;
        }

    private:

        BaseTime m_prevtime;
        Bool m_enabled;
        LONG m_level_override;

        typedef c4d_misc::BaseArray<SmearState> StateArray;
        StateArray m_states;

    };

    class SmearSession {

    public:

        SmearSession(SmearHistory* history, SmearState* state, Bool fake);

        ~SmearSession();

        Bool CreateState(
                const Matrix& mg,
                const Vector* vertices, LONG vertex_count,
                const CPolygon* faces, LONG face_count);

        Bool DeformationComplete(const Matrix& mg);

        Bool IsUpToDate() const { return m_updated; }

    private:

        SmearState* m_state;
        Bool m_fake;
        Bool m_created;
        Bool m_updated;

        const Vector* m_vertices;
        LONG m_vertex_count;
        const CPolygon* m_faces;
        LONG m_face_count;

    };

#endif /* NR_SMEARHISTORY_H */
