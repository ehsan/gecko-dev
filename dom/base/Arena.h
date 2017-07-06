/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef Arena_h___
#define Arena_h___

#include "nsArenaList.h"
#include "mozilla/ArenaObjectID.h"
#include "mozilla/ArenaRefPtr.h"

#define NS_DECL_DOMARENA_HELPERS_NO_DELETECYCLECOLLECTABLE                     \
  void* operator new(size_t aSize, nsNodeInfoManager* aManager);               \
  void  operator delete(void* aPtr);                                           \
  size_t SizeOfIncludingThis(mozilla::SizeOfState& aState) const               \
    override;

#define NS_DECL_DOMARENA_HELPERS                                               \
  NS_DECL_DOMARENA_HELPERS_NO_DELETECYCLECOLLECTABLE                           \
  NS_IMETHOD_(void) DeleteCycleCollectable() override;

#define NS_IMPL_DOMARENA_DELETECYCLECOLLECTABLE(class)                         \
  NS_IMETHODIMP_(void) class::DeleteCycleCollectable()                         \
  {                                                                            \
    if (MOZ_UNLIKELY(MayNodeBeOnAlternateArena())) {                           \
      /* If the node was adopted into another document, delete it from         \
         the original arena. */                                                \
      auto* nim = static_cast<nsNodeInfoManager*>                              \
        (GetProperty(nsGkAtoms::alternatearena));                              \
      nim->Free(sizeof(class), this);                                          \
      return;                                                                  \
    }                                                                          \
    OwnerDoc()->NodeInfoManager()->Free(sizeof(class), this);                  \
  }

#define NS_IMPL_DOMARENA_HELPERS_NO_DELETECYCLECOLLECTABLE(class)              \
  static_assert(sizeof(class) <= 512,                                          \
    "Classes larger than 512 bytes aren't supported in DOM arenas yet");       \
  void* class::operator new(size_t aSize,                                      \
                            nsNodeInfoManager* aManager)                       \
  {                                                                            \
    return aManager->Allocate(aSize);                                          \
  }                                                                            \
  void class::operator delete(void* aPtr) {}                                   \
  size_t class::SizeOfIncludingThis(mozilla::SizeOfState& aState) const        \
  {                                                                            \
    return nsNodeInfoManager::SizeOfNode(sizeof(class)) +                      \
           SizeOfExcludingThis(aState);                                        \
  }

#define NS_IMPL_DOMARENA_HELPERS(class)                                        \
  NS_IMPL_DOMARENA_HELPERS_NO_DELETECYCLECOLLECTABLE(class)                    \
  NS_IMPL_DOMARENA_DELETECYCLECOLLECTABLE(class)

namespace mozilla {
namespace dom {

enum class ArenaObjectID {
#ifndef HAVE_64BIT_BUILD
  // On 64-bit platforms, there are no nodes that are small enough to fit in this pool.
  SixtyFour_Byte,
#endif
  HundredTwentyEight_Byte,
  TwoHundredFiftySix_Byte,
  FiveHundredTwelve_Byte,
  Attribute,
  Count
};

class Arena :
  public nsArenaList<mozilla::dom::ArenaObjectID, false,
                     size_t(mozilla::dom::ArenaObjectID::Count),
                     8192, 8>
{
public:
  Arena() = default;
  ~Arena() = default;

  void* Allocate(size_t aSize)
  {
    return Base::Allocate(GetClassArena(aSize), GetBucketSize(aSize));
  }
  void Free(size_t aSize, void* aPtr)
  {
    Base::Free(GetClassArena(aSize), aPtr);
  }

  void* AllocateAttr(size_t aSize)
  {
    return Base::Allocate(uint32_t(ArenaObjectID::Attribute),
                          GetBucketSize(aSize));
  }
  void FreeAttr(void* aPtr)
  {
    Base::Free(uint32_t(ArenaObjectID::Attribute), aPtr);
  }

  static uint32_t GetClassArena(size_t aSize)
  {
    // Attributes are special-cased in the Attr class.

    size_t bucket = GetBucketSize(aSize);
    switch (bucket) {
#ifndef HAVE_64BIT_BUILD
    case 64:
      return uint32_t(ArenaObjectID::SixtyFour_Byte);
#endif
    case 128:
      return uint32_t(ArenaObjectID::HundredTwentyEight_Byte);
    case 256:
      return uint32_t(ArenaObjectID::TwoHundredFiftySix_Byte);
    case 512:
      return uint32_t(ArenaObjectID::FiveHundredTwelve_Byte);
    default:
      MOZ_ASSERT_UNREACHABLE("Encountered node with unsupported size!");
      return uint32_t(ArenaObjectID::Count);
    }
  }
  static size_t GetBucketSize(size_t aSizeOfType)
  {
    // Branch-less next power of two algorithm follows.
    size_t num = aSizeOfType;
    --num;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
#ifdef HAVE_64BIT_BUILD
    num |= num >> 32;
#endif
    ++num;
    return num;
  }

private:
  using Base = nsArenaList<mozilla::dom::ArenaObjectID, false,
                           size_t(mozilla::dom::ArenaObjectID::Count),
                           8192, 8>;
};

}
}

#endif
