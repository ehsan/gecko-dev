struct RefCounted {
  void AddRef();
  void Release();
};

void takeRefCounted(RefCounted*);
void takeVoidStar(void*);
void takeVariadic(...);

struct A : RefCounted {
  A();
  void f();
};
A::A() {
  takeRefCounted(this); // expected-warning {{`this' pointer cannot be passed as argument 0 to 'takeRefCounted' of type 'RefCounted *'}}
  takeRefCounted((this)); // expected-warning {{`this' pointer cannot be passed as argument 0 to 'takeRefCounted' of type 'RefCounted *'}}
  takeVoidStar(this);
  takeVariadic(this);
}
void A::f() {
  takeRefCounted(this);
  takeVoidStar(this);
  takeVariadic(this);
}

struct B : RefCounted {
  B() {
    takeRefCounted(this); // expected-warning {{`this' pointer cannot be passed as argument 0 to 'takeRefCounted' of type 'RefCounted *'}}
    takeRefCounted((this)); // expected-warning {{`this' pointer cannot be passed as argument 0 to 'takeRefCounted' of type 'RefCounted *'}}
    takeVoidStar(this);
    takeVariadic(this);
  }
  void f() {
    takeRefCounted(this);
    takeVoidStar(this);
    takeVariadic(this);
  }
};

struct C {
  C() {
    takeVoidStar(this);
    takeVariadic(this);
  }
  void f() {
    takeVoidStar(this);
    takeVariadic(this);
  }
};
