#define MOZ_HEAP_CLASS __attribute__((annotate("moz_heap_class")))
#include <stddef.h>

struct MOZ_HEAP_CLASS Heap {
  int i;
  void *operator new(size_t x) { return 0; }
  void *operator new(size_t blah, char *buffer) { return buffer; }
};

template <class T>
struct MOZ_HEAP_CLASS TemplateClass {
  T i;
};

void gobble(void *) { }

void misuseHeapClass(int len) {
  Heap invalid; // expected-error {{variable of type 'Heap' only valid on the heap}}
  Heap alsoInvalid[2]; // expected-error {{variable of type 'Heap [2]' only valid on the heap}}
  static Heap notValid; // expected-error {{variable of type 'Heap' only valid on the heap}}
  static Heap alsoNotValid[2]; // expected-error {{variable of type 'Heap [2]' only valid on the heap}}

  gobble(new Heap);
  gobble(new Heap[10]);
  gobble(new TemplateClass<int>);
  gobble(len <= 5 ? (Heap*)0 : new Heap);

  char buffer[sizeof(Heap)];
  gobble(new (buffer) Heap); // expected-error {{variable of type 'Heap' only valid on the heap}}
}

Heap notValid; // expected-error {{variable of type 'Heap' only valid on the heap}}
struct RandomClass {
  Heap nonstaticMember; // expected-note {{'RandomClass' is a heap class because member 'nonstaticMember' is a heap class 'Heap'}}
  static Heap staticMember; // expected-error {{variable of type 'Heap' only valid on the heap}}
};
struct MOZ_HEAP_CLASS RandomHeapClass {
  Heap nonstaticMember;
  static Heap staticMember; // expected-error {{variable of type 'Heap' only valid on the heap}}
};

struct BadInherit : Heap {}; // expected-note {{'BadInherit' is a heap class because it inherits from a heap class 'Heap'}}
struct MOZ_HEAP_CLASS GoodInherit : Heap {};

BadInherit moreInvalid; // expected-error {{variable of type 'BadInherit' only valid on the heap}}
RandomClass evenMoreInvalid; // expected-error {{variable of type 'RandomClass' only valid on the heap}}
