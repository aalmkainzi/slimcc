#include <stdio.h>

_Nameprefix A = "_1_";

_Apply _Nameprefix A
{
    int foo() { puts("hello"); }
}

int main()
{
    A::foo();
}