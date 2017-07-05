/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* arena allocation for the frame tree and closely-related objects */

#ifndef nsPresArena_h___
#define nsPresArena_h___

#include "nsArenaList.h"
#include "mozilla/ArenaObjectID.h"
#include "mozilla/ArenaRefPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"

class nsWindowSizes;

class nsPresArena :
  public nsArenaList<mozilla::ArenaObjectID, true,
                     mozilla::eArenaObjectID_COUNT,
                     8192, 8>
{
public:
  nsPresArena();
  ~nsPresArena();

  /**
   * Pool allocation with recycler lists indexed by frame-type ID.
   * Every aID must always be used with the same object size, aSize.
   */
  void* AllocateByFrameID(nsQueryFrame::FrameIID aID, size_t aSize)
  {
    return Allocate(aID, aSize);
  }
  void FreeByFrameID(nsQueryFrame::FrameIID aID, void* aPtr)
  {
    Free(aID, aPtr);
  }

  /**
   * Register an ArenaRefPtr to be cleared when this arena is about to
   * be destroyed.
   *
   * (Defined in ArenaRefPtrInlines.h.)
   *
   * @param aPtr The ArenaRefPtr to clear.
   * @param aObjectID The ArenaObjectID value that uniquely identifies
   *   the type of object the ArenaRefPtr holds.
   */
  template<typename T>
  void RegisterArenaRefPtr(mozilla::ArenaRefPtr<T>* aPtr);

  /**
   * Deregister an ArenaRefPtr that was previously registered with
   * RegisterArenaRefPtr.
   */
  template<typename T>
  void DeregisterArenaRefPtr(mozilla::ArenaRefPtr<T>* aPtr)
  {
    MOZ_ASSERT(mArenaRefPtrs.Contains(aPtr));
    mArenaRefPtrs.Remove(aPtr);
  }

  /**
   * Clears all currently registered ArenaRefPtrs.  This will be called during
   * the destructor, but can be called by users of nsPresArena who want to
   * ensure arena-allocated objects are released earlier.
   */
  void ClearArenaRefPtrs();

  /**
   * Clears all currently registered ArenaRefPtrs for the given ArenaObjectID.
   * This is called when we reconstruct the rule tree so that style contexts
   * pointing into the old rule tree aren't released afterwards, triggering an
   * assertion in ~nsStyleContext.
   */
  void ClearArenaRefPtrs(mozilla::ArenaObjectID aObjectID);

  /**
   * Increment nsWindowSizes with sizes of interesting objects allocated in this
   * arena.
   */
  void AddSizeOfExcludingThis(nsWindowSizes& aWindowSizes) const;

private:
  inline void ClearArenaRefPtrWithoutDeregistering(
      void* aPtr,
      mozilla::ArenaObjectID aObjectID);

  nsDataHashtable<nsPtrHashKey<void>, mozilla::ArenaObjectID> mArenaRefPtrs;
};

#endif
