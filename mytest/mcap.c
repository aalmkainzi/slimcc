_Nameprefix A = "shared_";
_Nameprefix B = "shared_";
_Nameprefix C = "other_";

_Capture _Nameprefix A {
    _Capture _Nameprefix B {
        _Capture _Nameprefix C {
            void shared_func() {} 
            // 1. Matches A's prefix ("shared_") -> Added to A::func
            // 2. Matches B's prefix ("shared_") -> Added to B::func
            // 3. Does NOT match C's prefix ("other_") -> Ignored by C
            
            void other_stuff() {}
            // 1. Matches C's prefix -> Added to C::stuff
            // 2. Ignored by A and B
        }
    }
}

int a() {
    A::func(); // OK
    B::func(); // OK
    // C::func(); // ERROR
    C::stuff(); // OK
}