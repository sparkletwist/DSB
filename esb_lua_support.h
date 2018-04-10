void esb_set_level_wallset(lua_State *LUA, int lev_on, const char *ws_name);
void esb_set_loc_wallset(lua_State *LUA,
    int lev, int x, int y, int t, const char *ws_name);
void change_gfxflag(int inst, int flag, int operation);
void enable_inst(unsigned int inst, int onoff);
