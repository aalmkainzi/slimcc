_Nameprefix Net = "net_"; // Valid: Outer-most scope

// _Apply _Nameprefix Unknown { } // ERROR: Unknown not previously declared

_Apply _Nameprefix Net {
    void setup() {
        // ping(); // ERROR: 'ping' is not a member of 'Net' yet
    }
    
    void ping() {} // Exported as net_ping
    
    void start() {
        ping(); // VALID: 'ping' is a known member of 'Net' now
    }
}

int main(){}