_Nameprefix A = "A_";
_Nameprefix A::B = "A_B_";

_Apply _Nameprefix A
{
    _Apply _Nameprefix B
    {
        int foo(){ return 10; }
    }
    
    _Capture _Nameprefix _Global
    {
        int func() { return 6; }
    }
}

int b()
{
    return
    _Global::A::B::foo() + 
    _Global::func() +
    A::B::foo() +
    func();
}
