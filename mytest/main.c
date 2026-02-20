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

_Capture _Nameprefix B
{
    enum B__XXX { B__H, I, J };
}

int main()
{
    return B::H;
}
