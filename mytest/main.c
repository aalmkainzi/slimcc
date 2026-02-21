_Nameprefix Physics = "phys_";
_Nameprefix Physics::Engine = "phys_";
_Nameprefix Physics::Engine::Collision = "phys_col_";
_Nameprefix col = Physics::Engine::Collision; // Alias

_Apply _Nameprefix Physics::Engine::Collision
{
    void detect() {} // phys_col_detect
}

int main() {
    Physics::Engine::Collision::detect(); // Long path
    col::detect();                        // Alias path
    _Nameprefix col2 = col;
    phys_col_detect();                    // Direct mangled path
    return 0;
}