_Nameprefix A = "A__";
_Nameprefix K = "K__";
_Nameprefix K::A = "K__A__";

_Apply _Nameprefix K::A
{
    int foo() { return 3; }
}

_Apply _Nameprefix A
{
    int foo() { return 1; }
}

_Apply _Nameprefix K
{
    int x()
    {
        return _Global::A::foo();
    }
}

int main()
{
    return K::x();
}
