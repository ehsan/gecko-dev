#define MOZ_STRONG_REF __attribute__((annotate("moz_strong_ref")))
#define MOZ_WEAK_REF __attribute__((annotate("moz_weak_ref")))

struct RefCountedBase {
  void AddRef();
  void Release();
};

template <class T>
struct SmartPtr {
  T* MOZ_STRONG_REF t;
};

struct R : RefCountedBase {};

struct BadMembers {
  R* r; // expected-error {{Raw pointer member 'r' points to refcounted class 'R'}} expected-note{{Please use the appropriate smart pointer class (such as nsCOMPtr, RefPtr)}}
  const R* r2; // expected-error {{Raw pointer member 'r2' points to refcounted class 'const R'}} expected-note{{Please use the appropriate smart pointer class (such as nsCOMPtr, RefPtr)}}
  R* const r3; // expected-error {{Raw pointer member 'r3' points to refcounted class 'R'}} expected-note{{Please use the appropriate smart pointer class (such as nsCOMPtr, RefPtr)}}
  const R* const r4; // expected-error {{Raw pointer member 'r4' points to refcounted class 'const R'}} expected-note{{Please use the appropriate smart pointer class (such as nsCOMPtr, RefPtr)}}
};

template <class T>
struct BadMembersT {
  R* r; // expected-error {{Raw pointer member 'r' points to refcounted class 'R'}} expected-note{{Please use the appropriate smart pointer class (such as nsCOMPtr, RefPtr)}}
  const R* r2; // expected-error {{Raw pointer member 'r2' points to refcounted class 'const R'}} expected-note{{Please use the appropriate smart pointer class (such as nsCOMPtr, RefPtr)}}
  R* const r3; // expected-error {{Raw pointer member 'r3' points to refcounted class 'R'}} expected-note{{Please use the appropriate smart pointer class (such as nsCOMPtr, RefPtr)}}
  const R* const r4; // expected-error {{Raw pointer member 'r4' points to refcounted class 'const R'}} expected-note{{Please use the appropriate smart pointer class (such as nsCOMPtr, RefPtr)}}
};

struct GoodMembers {
  SmartPtr<R> r;
  R* MOZ_STRONG_REF r2;
  R* MOZ_WEAK_REF r3;
  R& r4;
};
