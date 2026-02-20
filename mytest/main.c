_Nameprefix A = "A__";
_Nameprefix B = "B__";

_Apply _Nameprefix A
{
    enum XX { A, B, C };
}

_Apply _Nameprefix B
{
    enum XX { D, E, F };
}

int main()
{
    return A::A + B::F;
}
