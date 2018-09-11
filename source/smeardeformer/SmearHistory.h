/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * SmearHistory.h
 */

#ifndef NR_SMEARHISTORY_H
#define NR_SMEARHISTORY_H

    #include <c4d.h>

    class SmearSession;

    struct SmearState {

        Vector* original_vertices;
        Vector* original_normals;
        Vector* deformed_vertices;
        Vector* deformed_normals;
        Float32*  weights; // filled by the smear deformer
        Matrix mg;
        Int32 vertex_count;
        Bool initialized;

        SmearState()
        : original_vertices(nullptr), original_normals(nullptr),
          deformed_vertices(nullptr), deformed_normals(nullptr),
          weights(nullptr), vertex_count(0), initialized(false) { }

        ~SmearState() {
        }

        Bool Resize(Int32 count);

        void Flush();

    };

    class SmearHistory {

        friend class SmearSession;

    public:

        SmearHistory()
        : m_enabled(true), m_level_override(-1) { }

        ~SmearHistory();

        SmearSession* NewSession(Int32 max_history_count, Bool fake_session=false);

        void FreeSession(SmearSession*& session);

        Int32 GetHistoryCount() const;

        const SmearState* GetState(Int32 index) const;

        void SetEnabled(Bool enabled) { m_enabled = enabled; }

        void Reset();

        Bool CompareTime(const BaseTime& time) {
            if (m_prevtime != time) {
                m_prevtime = time;
                return true;
            }
            return false;
        }

        void CapHistoryLevel(Int32 history_level) {
            if (history_level < 1) history_level = 1;
            m_level_override = history_level + 1;
        }

    private:

        BaseTime m_prevtime;
        Bool m_enabled;
        Int32 m_level_override;

        typedef maxon::BaseArray<SmearState> StateArray;
        StateArray m_states;

    };

    class SmearSession {

    public:

        SmearSession(SmearHistory* history, SmearState* state, Bool fake);

        ~SmearSession();

        Bool CreateState(
                const Matrix& mg,
                const Vector* vertices, Int32 vertex_count,
                const CPolygon* faces, Int32 face_count);

        Bool DeformationComplete(const Matrix& mg);

        Bool IsUpToDate() const { return m_updated; }

    private:

        SmearState* m_state;
        Bool m_fake;
        Bool m_created;
        Bool m_updated;

        const Vector* m_vertices;
        Int32 m_vertex_count;
        const CPolygon* m_faces;
        Int32 m_face_count;

    };

#endif /* NR_SMEARHISTORY_H */
