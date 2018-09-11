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

    namespace nr {
    namespace memory {

        template <typename T>
        T* Alloc() {
            return (T*) NewMem(char, sizeof(T));
        }

        template <typename T>
        T* Alloc(Int32 count) {
            iferr (auto&& res = NewMem(char, sizeof(T) * count))
                return nullptr;
            return (T*) res;
        }

        template <typename T>
        Bool Realloc(T*& ptr, Int32 count) {
            if (ptr) {
                iferr (auto&& res = ReallocMem(ptr, sizeof(T) * count))
                    return false;
                ptr = (T*) res;
            }
            else {
                ptr = (T*) nr::memory::Alloc<T>(count);
            }
            return ptr != nullptr;
        }

        template <typename T>
        void Free(T*& ptr) {
            if (ptr) {
                DeleteMem((void*&) ptr);
                ptr = nullptr;
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

            Int32 m_alloc;
            Int32 m_dealloc;

        public:

            inline Int32 GetAllocationCount() const { return m_alloc; }

            inline Int32 GetDeallocationCount() const { return m_dealloc; }

            inline Int32 GetAliveCount() const { return m_alloc - m_dealloc; }

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
