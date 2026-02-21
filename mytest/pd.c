_Nameprefix Task = "t_";

_Apply _Nameprefix Task {
    void finish() {}
    void start() {
        Task::finish();
        finish();
        _Global::Task::finish();
        t_finish();
        // _Global::t_finish(); // this currently is not allowed
    }
    
}