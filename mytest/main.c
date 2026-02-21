_Nameprefix C1 = "C__";
_Nameprefix C2 = "C__";

_Nameprefix C1::A = "C__";

_Capture _Nameprefix
C1,
C1::A
{
    float C__i = 5;
    int C__foo() { return 20; }
}

float i = 10;

_Apply _Nameprefix C1
{
    struct K {int d;} x1;
    
    float baba()
    {
        return i;
    }
}

int main()
{
    struct C1::K k = C1::A::x1;
}
