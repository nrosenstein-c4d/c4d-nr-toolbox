/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * nrUtils/Memory.h
 *
 * created: 2013-07-19
 * updated: 2013-08-26
 */

#ifndef NR_UTILS_ALLOCATION_H
#define NR_UTILS_ALLOCATION_H

    #include "misc/legacy.h"

    namespace nr {
    namespace memory {

        template <typename T>
        T* Alloc() {
            return (T*) NewMem(char, sizeof(T));
        }

        template <typename T>
        T* Alloc(LONG count) {
            return (T*) NewMem(char, sizeof(T) * count);
        }

        template <typename T>
        Bool Realloc(T*& ptr, LONG count) {
            if (ptr) {
                ptr = (T*) ReallocMem(ptr, sizeof(T) * count);
            }
            else {
                ptr = (T*) nr::memory::Alloc<T>(count);
            }
            return ptr != NULL;
        }

        template <typename T>
        void Free(T*& ptr) {
            if (ptr) {
                DeleteMem((void*&) ptr);
                ptr = NULL;
            }
        }

        /**
         * Abstract class for allocating and deallocating memory.
         */
        class MemoryManager {

        public:

            virtual void* AllocMemory(size_t size) = 0;

            virtual void FreeMemory(void* ptr) = 0;

        public:

            /**
             * Allocate a new object of the passed template type using the
             * memory allocated with the called @class`MemoryManager`.
             */
            template <typename T>
            T* NewObject() {
                T* obj = (T*) AllocMemory(sizeof(T));
                if (obj) new(obj) T();
                return obj;
            }

            /**
             * Invokes an object's virtual destructor and deletes it using
             * the implemented @method`FreeMemory()` method.
             */
            template <typename T>
            void DelObject(T* ptr) {
                if (ptr) {
                    ptr->~T();
                    FreeMemory(ptr);
                }
            }

        };

        /**
         * Base class for memory managers that perform allocation counting.
         */
        class CountingMemoryManager : public MemoryManager {

        protected:

            LONG m_alloc;
            LONG m_dealloc;

        public:

            inline LONG GetAllocationCount() const { return m_alloc; }

            inline LONG GetDeallocationCount() const { return m_dealloc; }

            inline LONG GetAliveCount() const { return m_alloc - m_dealloc; }

        };

        /**
         * This @class`MemoryManager` implementation makes use of Cinema 4D's
         * memory management mechanisms.
         */
        extern CountingMemoryManager* C4DMem;

        /**
         * This @class`MemoryManager` implementation uses the standart library
         * memory management functions.
         */
        extern CountingMemoryManager* STDMem;

    } // namespace memory
    } // namespace nr

#endif /* NR_UTILS_ALLOCATION_H */
