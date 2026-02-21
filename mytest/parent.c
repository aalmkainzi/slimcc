_Nameprefix Parent = "P_";
_Nameprefix Parent::Child = "P_C_"; // Declared at file scope
_Nameprefix Unrelated = "U_";

_Apply _Nameprefix Parent {
    _Apply _Nameprefix Child {
        void work() {} // P_C_work
    }
    
     
    // _Apply _Nameprefix Parent { 
    //     // ERROR: Unrelated is not a child of Parent
    // } 
    
}

int main(){}