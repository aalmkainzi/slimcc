_Nameprefix Lib = "L_";

_Capture _Nameprefix Lib {
    // We are at global scope. 'Internal' doesn't exist here. 
    // We must declare it as a child of Lib.
    _Nameprefix Lib::Internal = "L_i_"; 
    
    void L_init() {} // Captured into Lib
    
    // _Apply _Nameprefix Internal { ... } // ERROR: 'Internal' is not in global scope.
    
    _Apply _Nameprefix Lib::Internal { 
        void secret() {} // Exported as L_i_secret
    }
}