/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef nsArenaList_h___
#define nsArenaList_h___

#include "mozilla/ArenaAllocator.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/MemoryChecking.h" // Note: Do not remove this, needed for MOZ_HAVE_MEM_CHECKS below
#include "mozilla/MemoryReporting.h"
#include "mozilla/Poison.h"
#include <stdint.h>
#include "nscore.h"
#include "nsTArray.h"
#include "nsPrintfCString.h"

template <class  ArenaObjectID,
          bool   EnablePoisoning,
          size_t MaxBuckets,
          size_t ArenaSize,
          size_t Alignment>
class nsArenaList
{
public:
  nsArenaList() = default;
  ~nsArenaList() = default;

  /**
   * Pool allocation with recycler lists indexed by object-type ID (see above).
   * Every aID must always be used with the same object size, aSize.
   */
  void* AllocateByObjectID(ArenaObjectID aID, size_t aSize)
  {
    return Allocate(aID, aSize);
  }
  void FreeByObjectID(ArenaObjectID aID, void* aPtr)
  {
    Free(aID, aPtr);
  }

protected:
  void* Allocate(uint32_t aCode, size_t aSize)
  {
    MOZ_ASSERT(aSize > 0, "PresArena cannot allocate zero bytes");
    MOZ_ASSERT(aCode < mozilla::ArrayLength(mFreeLists));

    // We only hand out aligned sizes
    aSize = mPool.AlignedSize(aSize);

    FreeList* list = &mFreeLists[aCode];

    if (void* result = list->GetFreeEntry(aSize)) {
      return result;
    }

    // Allocate a new chunk from the arena
    list->NoteAllocated();
    return mPool.Allocate(aSize);
  }
  void Free(uint32_t aCode, void* aPtr)
  {
    MOZ_ASSERT(aCode < mozilla::ArrayLength(mFreeLists));

    // Try to recycle this entry.
    FreeList* list = &mFreeLists[aCode];
    MOZ_ASSERT(list->EntrySize() > 0, "object of this type was never allocated");

    if (EnablePoisoning) {
      mozWritePoison(aPtr, list->EntrySize());
    }

    MOZ_MAKE_MEM_NOACCESS(aPtr, list->EntrySize());
    list->Add(aPtr);
  }

  class FreeList
  {
    nsTArray<void *> mEntries;
  public:
    size_t mEntrySize;
    size_t mEntriesEverAllocated;

    FreeList()
      : mEntrySize(0)
      , mEntriesEverAllocated(0)
    {}

    ~FreeList()
    {
#if defined(MOZ_HAVE_MEM_CHECKS)
      nsTArray<void*>::index_type len;
      while ((len = mEntries.Length())) {
        void* result = mEntries.ElementAt(len - 1);
        mEntries.RemoveElementAt(len - 1);
        MOZ_MAKE_MEM_UNDEFINED(result, mEntrySize);
      }
#endif
    }

    size_t EntrySize() const { return mEntrySize; }

    void Add(void* aPtr)
    {
      mEntries.AppendElement(aPtr);
    }

    void NoteAllocated()
    {
      mEntriesEverAllocated++;
    }

    // Returns non-null if the free list contains a recylcled entry.
    void* GetFreeEntry(size_t aSize)
    {
      nsTArray<void*>::index_type len = mEntries.Length();
      if (mEntrySize == 0) {
        MOZ_ASSERT(len == 0, "list with entries but no recorded size");
        mEntrySize = aSize;
      } else {
        MOZ_ASSERT(mEntrySize == aSize,
                   "different sizes for same object type code");
      }

      void* result;
      if (len > 0) {
        // Remove from the end of the mEntries array to avoid memmoving entries,
        // and use SetLengthAndRetainStorage to avoid a lot of malloc/free
        // from ShrinkCapacity on smaller sizes.  500 pointers means the malloc size
        // for the array is 4096 bytes or more on a 64-bit system.  The next smaller
        // size is 2048 (with jemalloc), which we consider not worth compacting.
        result = mEntries.ElementAt(len - 1);
        if (mEntries.Capacity() > 500) {
          mEntries.RemoveElementAt(len - 1);
        } else {
          mEntries.SetLengthAndRetainStorage(len - 1);
        }
#if defined(DEBUG)
        MOZ_MAKE_MEM_DEFINED(result, mEntrySize);
        if (EnablePoisoning) {
          char* p = reinterpret_cast<char*>(result);
          char* limit = p + mEntrySize;
          for (; p < limit; p += sizeof(uintptr_t)) {
            uintptr_t val = *reinterpret_cast<uintptr_t*>(p);
            if (val != mozPoisonValue()) {
              MOZ_ReportAssertionFailure(
                nsPrintfCString("nsArenaList: poison overwritten; "
                                "wanted %.16" PRIx64 " "
                                "found %.16" PRIx64 " "
                                "errors in bits %.16" PRIx64 " ",
                                uint64_t(mozPoisonValue()),
                                uint64_t(val),
                                uint64_t(mozPoisonValue() ^ val)).get(),
                __FILE__, __LINE__);
              MOZ_CRASH();
            }
          }
        }
#endif
        MOZ_MAKE_MEM_UNDEFINED(result, mEntrySize);
        return result;
      }
      return nullptr;
    }

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
    { return mEntries.ShallowSizeOfExcludingThis(aMallocSizeOf); }
  };

  FreeList mFreeLists[MaxBuckets];
  mozilla::ArenaAllocator<ArenaSize, Alignment> mPool;
};

#endif
