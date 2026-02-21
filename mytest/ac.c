_Nameprefix GUI = "gui_";

_Capture _Nameprefix GUI {
    void gui_draw_button() {} // Captured

    _Apply _Nameprefix GUI {
        void render() {
            draw_button(); // VALID: Shorthand works here because of _Apply
        }
    }
    
    void gui_update() {
        // render(); // ERROR: Shorthand is gone; we are back in _Capture (global)
        GUI::render(); // VALID
    }
}