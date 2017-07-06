/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* arena allocation for the frame tree and closely-related objects */

#include "nsPresArena.h"

#include "nsDebug.h"
#include "nsPrintfCString.h"
#include "GeckoStyleContext.h"
#include "FrameLayerBuilder.h"
#include "mozilla/ArrayUtils.h"
#include "nsStyleContext.h"
#include "nsStyleContextInlines.h"
#include "nsWindowSizes.h"

#include <inttypes.h>

using namespace mozilla;

nsPresArena::nsPresArena()
{
}

nsPresArena::~nsPresArena()
{
  ClearArenaRefPtrs();
}


/* inline */ void
nsPresArena::ClearArenaRefPtrWithoutDeregistering(void* aPtr,
                                                  mozilla::ArenaObjectID aObjectID)
{
  switch (aObjectID) {
    // We use ArenaRefPtr<nsStyleContext>, which can be ServoStyleContext
    // or GeckoStyleContext. GeckoStyleContext is actually arena managed,
    // but ServoStyleContext isn't.
    case eArenaObjectID_GeckoStyleContext:
      static_cast<ArenaRefPtr<nsStyleContext>*>(aPtr)->ClearWithoutDeregistering();
      return;
    default:
      MOZ_ASSERT(false, "unexpected ArenaObjectID value");
      break;
  }
}

void
nsPresArena::ClearArenaRefPtrs()
{
  for (auto iter = mArenaRefPtrs.Iter(); !iter.Done(); iter.Next()) {
    void* ptr = iter.Key();
    mozilla::ArenaObjectID id = iter.UserData();
    ClearArenaRefPtrWithoutDeregistering(ptr, id);
  }
  mArenaRefPtrs.Clear();
}

void
nsPresArena::ClearArenaRefPtrs(mozilla::ArenaObjectID aObjectID)
{
  for (auto iter = mArenaRefPtrs.Iter(); !iter.Done(); iter.Next()) {
    void* ptr = iter.Key();
    mozilla::ArenaObjectID id = iter.UserData();
    if (id == aObjectID) {
      ClearArenaRefPtrWithoutDeregistering(ptr, id);
      iter.Remove();
    }
  }
}

void
nsPresArena::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const
{
  // We do a complicated dance here because we want to measure the
  // space taken up by the different kinds of objects in the arena,
  // but we don't have pointers to those objects.  And even if we did,
  // we wouldn't be able to use mMallocSizeOf on them, since they were
  // allocated out of malloc'd chunks of memory.  So we compute the
  // size of the arena as known by malloc and we add up the sizes of
  // all the objects that we care about.  Subtracting these two
  // quantities gives us a catch-all "other" number, which includes
  // slop in the arena itself as well as the size of objects that
  // we've not measured explicitly.

  size_t mallocSize = mPool.SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

  size_t totalSizeInFreeLists = 0;
  for (const FreeList* entry = mFreeLists;
       entry != ArrayEnd(mFreeLists);
       ++entry) {
    mallocSize += entry->SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

    // Note that we're not measuring the size of the entries on the free
    // list here.  The free list knows how many objects we've allocated
    // ever (which includes any objects that may be on the FreeList's
    // |mEntries| at this point) and we're using that to determine the
    // total size of objects allocated with a given ID.
    size_t totalSize = entry->mEntrySize * entry->mEntriesEverAllocated;
    size_t* p;

    switch (entry - mFreeLists) {
#define FRAME_ID(classname, ...) \
      case nsQueryFrame::classname##_id: \
        p = &aSizes.mArenaSizes.NS_ARENA_SIZES_FIELD(classname); \
        break;
#define ABSTRACT_FRAME_ID(...)
#include "nsFrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID
      case eArenaObjectID_nsLineBox:
        p = &aSizes.mArenaSizes.mLineBoxes;
        break;
      case eArenaObjectID_nsRuleNode:
        p = &aSizes.mArenaSizes.mRuleNodes;
        break;
      case eArenaObjectID_GeckoStyleContext:
        p = &aSizes.mArenaSizes.mStyleContexts;
        break;
#define STYLE_STRUCT(name_, checkdata_cb_)      \
        case eArenaObjectID_nsStyle##name_:
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
        p = &aSizes.mArenaSizes.mStyleStructs;
        break;
      default:
        continue;
    }

    *p += totalSize;
    totalSizeInFreeLists += totalSize;
  }

  aSizes.mLayoutPresShellSize += mallocSize - totalSizeInFreeLists;
}
