_Nameprefix Lib = "LIB__";

_Apply _Nameprefix Lib
{
    int foo() // exported as LIB__foo
    {
        return 0;
    }
}

_Capture _Nameprefix Lib
{
    int LIB__bar() // enters Lib as bar
    {
        return 1;
    }
}

int main()
{
    int w = Lib::foo();
    int x = Lib::bar();
    // can also use prefixed names:
    int y = LIB__foo();
    int z = LIB__bar();
    
    return w + x + y + z;
}