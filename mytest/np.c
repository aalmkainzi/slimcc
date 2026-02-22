_Nameprefix A = "A__";
_Nameprefix A::B = "B__";

_Nameprefix ab = A::B;

_Apply _Nameprefix A
{
    int foo() { return 5; }
}

_Nameprefix g = _Global;

_Apply _Nameprefix A::B
{
    int bar() { return 10; }
}

int main()
{
    _Nameprefix tmp = ab;
    _Nameprefix ab = ab;
    
    return ab::bar() + tmp::bar() + A::B::bar() + g::A__foo() + g::B__bar();
}
