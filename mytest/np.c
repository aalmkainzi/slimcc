_Nameprefix A = "A__";
_Nameprefix A::B = "B__";

_Nameprefix ab = A::B;

_Apply _Nameprefix A
{
    int foo() { return 5; }
}

_Apply _Nameprefix A::B
{
    int bar() { return 10; }
}

int main()
{
    return ab::bar();
}
