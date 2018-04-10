#define STARTNONF() i_s_error_c = gd.lua_error_found, gd.lua_nonfatal = 100

#define ENDNONF() gd.lua_nonfatal = 0; \
if (gd.lua_error_found > i_s_error_c) { RETURN(0); }

#define NEWL(F) lua_pushcfunction(LUA, F);lua_setglobal(LUA, funcnames[Counter++]);
//    fprintf(errorlog, "FUNC: associating %s with %s\n", funcnames[Counter-1],#F);

#define INITPARMCHECK(P,F) luaonstack(F);if(lua_gettop(LUA)!=P) parmerror(P,F)

#define INITVPARMCHECK(P,F) luaonstack(F);if(lua_gettop(LUA)!= P && \
    lua_gettop(LUA)!= P+1) varparmerror(P,P+1,F);\
    if (lua_gettop(LUA) == P+1 && lua_isnil(LUA, -1)) lua_pop(LUA, 1)
    
#define INITRPARMCHECK(P,X,F) luaonstack(F);if(lua_gettop(LUA)!= P && \
    lua_gettop(LUA)!= X) varorparmerror(P,X,F)

//#define NotEditor(X) if(gd.edit_mode) {if(X) lua_pushnil(LUA);RETURN(1);}
//#define NotEditor(X) /* X */

#define only_when_loading(L, F) if (C_only_when_loading(L, F)) { RETURN(0); }
#define only_when_objparsing(L, F) if (C_only_when_objparsing(L, F)) { RETURN(0); }
#define only_when_dungeon(L, F) if (C_only_when_dungeon(L, F)) { RETURN(0); }
#define only_when_load_or_obj(L, F) if (C_only_when_load_or_obj(L, F)) { RETURN(0); }
