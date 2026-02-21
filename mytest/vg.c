_Nameprefix Net = "net_";
void raw_send() {} // _Global::raw_send

_Apply _Nameprefix Net {
    void send() {
        _Global::raw_send();
    }
    
    _Capture _Nameprefix Net {
        void net_connect() {
            // raw_send(); // VALID: In global scope via _Capture
            _Global::raw_send(); // ALSO VALID
        }
    }
}