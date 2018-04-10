void lua_register_funcs(lua_State *LUA);
const char *luastring(lua_State *LUA, int stackarg, const char*, int);
int luaint(lua_State *LUA, int stackarg, const char*, int);
unsigned int luargbval(lua_State *LUA, int stackarg,const char*, int);

unsigned int luaobjarch(lua_State *LUA, int stackarg, const char*);

struct animap *luabitmap(lua_State *LUA, int stackarg, const char*, int);
struct animap *luaoptbitmap(lua_State *LUA, int stackarg, char*);
FONT *luafont(lua_State *LUA, int stackarg, char *fname, int paramnum);

int luawallset(lua_State *LUA, int stackarg, const char*);

unsigned char luacharglobal(const char *gname);

void init_lua_font_handle(void);
void reget_lua_font_handle(void);

struct animap *grab_desktop(void);

