_Nameprefix Physics = "ph_";

_Capture _Nameprefix Physics {
    void ph_gravity() {}
    
    _Capture _Nameprefix Physics {
        // This is valid because _Capture resets to the global scope.
        // It's essentially redundant but allowed.
        
        void ph_friction() {
            Physics::gravity(); // Must use explicit access or full name
        }
    }
}