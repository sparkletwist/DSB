enum {
    _DSB_REGISTER_OBJECT,
    _DSB_NEWIDXFUNC,
    _DSB_LOG,
    _DSB_FLUSH,
    _DSB_NEXTINST,
    _DSB_L_TAG,
    _DSB_TRT_ASSIGN,
    _DSB_TRT_PURGE,
    _DSB_TRT_OBJ_WRITEBACK,
    _DSB_ACTIVATE_GUI,
    _DSB_SET_INTERNAL_GUI,
    _DSB_SET_INTERNAL_SHADEINFO,
    DSB_VALID_INST,
    DSB_GAME_END,
    DSB_INCLUDE_FILE,
    DSB_GET_BITMAP,
    DSB_CLONE_BITMAP,
    DSB_GET_MASK_BITMAP,
    DSB_GET_SOUND,
    DSB_MUSIC,
    DSB_MUSIC_TO_BACKGROUND,
    DSB_GET_FONT,
    DSB_DESTROY_BITMAP,
    DSB_NEW_BITMAP,
    DSB_DESKTOP_BITMAP,
    DSB_BITMAP_BLIT,
    DSB_BITMAP_WIDTH,
    DSB_BITMAP_HEIGHT,
    DSB_BITMAP_TEXTOUT,
    DSB_TEXT_SIZE,
    DSB_BITMAP_DRAW,
    DSB_BITMAP_RECT,
    DSB_GET_PIXEL,
    DSB_SET_PIXEL,
    DSB_RGB_TO_HSV,
    DSB_HSV_TO_RGB,
    DSB_CACHE_INVALIDATE,
    DSB_VIEWPORT,
    DSB_VIEWPORT_DISTORT,
    DSB_MAKE_WALLSET,
    DSB_MAKE_WALLSET_EXT,
    DSB_IMAGE2MAP,
    DSB_TEXT2MAP,
    DSB_PARTYPLACE,
    DSB_USE_WALLSET,
    DSB_LEVEL_FLAGS,
    DSB_LEVEL_TINT,
    DSB_LEVEL_GETINFO,
    DSB_SPAWN,
    DSB_MOVE,
    DSB_MOVE_MONCOL,
    DSB_REPOSITION,
    DSB_TILEPTR_EXCH,
    DSB_TILEPTR_ROTATE,
    DSB_COLLIDE,
    DSB_DELETE,
    DSB_ENABLE,
    DSB_DISABLE,
    DSB_TOGGLE,
    DSB_GET_CELL,
    DSB_SET_CELL,
    DSB_VISITED,
    DSB_FORWARD,
    DSB_GET_CONDITION,
    DSB_SET_CONDITION,
    DSB_ADD_CONDITION,
    DSB_REPLACE_CONDITION,
    DSB_ADD_CHAMPION,
    DSB_PUT_PPOS_TO_RELPOS,
    DSB_FETCH,
    DSB_QSWAP,
    DSB_SWAP,
    DSB_OFFER_CHAMPION,
    DSB_GET_GFXFLAG,
    DSB_SET_GFXFLAG,
    DSB_CLEAR_GFXFLAG,
    DSB_TOGGLE_GFXFLAG,
    DSB_GET_MFLAG,
    DSB_SET_MFLAG,
    DSB_CLEAR_MFLAG,
    DSB_TOGGLE_MFLAG,
    DSB_GET_CHARGE,
    DSB_SET_CHARGE, 
    DSB_GET_ANIMTIMER,
    DSB_SET_ANIMTIMER,
    DSB_GET_CROP,
    DSB_SET_CROP,
    DSB_GET_CRMOTION,
    DSB_SET_CRMOTION,
    DSB_GET_CROPMAX,
    DSB_SUB_TARG,
    DSB_DUNGEON_VIEW,
    DSB_BITMAP_CLEAR,
    DSB_OBJZONE,
    DSB_MSGZONE,
    DSB_FIND_ARCH,
    DSB_PARTY_COORDS,
    DSB_PARTY_AT,
    DSB_PARTY_CONTAINS,
    DSB_PARTY_VIEWING,
    DSB_PARTY_AFFECTING,
    DSB_PARTY_APUSH,
    DSB_PARTY_APOP,
    DSB_HIDE_MOUSE,
    DSB_SHOW_MOUSE,
    DSB_MOUSE_STATE,
    DSB_MOUSE_OVERRIDE,
    DSB_PUSH_MOUSE,
    DSB_POP_MOUSE,
    DSB_SHOOT,
    DSB_ANIMATE,
    DSB_GET_COORDS,
    DSB_GET_COORDS_PREV,
    DSB_SOUND,
    DSB_3DSOUND,
    DSB_STOPSOUND,
    DSB_CHECKSOUND,
    DSB_GET_SOUNDVOL,
    DSB_SET_SOUNDVOL,
    DSB_GET_FLYSTATE,
    DSB_SET_FLYSTATE,
    DSB_GET_SLEEPSTATE,
    DSB_WAKEUP,
    DSB_SET_OPENSHOT,
    DSB_GET_FLYDELTA,
    DSB_SET_FLYDELTA,
    DSB_GET_FLYREPS,
    DSB_SET_FLYREPS,
    DSB_GET_FACEDIR,
    DSB_SET_FACEDIR,
    DSB_GET_HP,
    DSB_SET_HP,
    DSB_GET_MAXHP,
    DSB_SET_MAXHP,
    DSB_SET_TINT,
    DSB_GET_PFACING,
    DSB_SET_PFACING,
    DSB_GET_CHARNAME,
    DSB_GET_CHARTITLE,
    DSB_SET_CHARNAME,
    DSB_SET_CHARTITLE,
    DSB_GET_LOAD,
    DSB_GET_MAXLOAD,
    DSB_GET_FOOD,
    DSB_SET_FOOD,
    DSB_GET_WATER,
    DSB_SET_WATER,
    DSB_GET_IDLE,
    DSB_SET_IDLE,
    DSB_GET_STAT,
    DSB_SET_STAT,
    DSB_GET_MAXSTAT,
    DSB_SET_MAXSTAT,
    DSB_GET_INJURY,
    DSB_SET_INJURY,
    DSB_UPDATE_II,
    DSB_UPDATE_SYSTEM,
    DSB_CUR_INV,
    DSB_DELAY_FUNC,
    DSB_WRITE,
    DSB_GET_BAR,
    DSB_SET_BAR,
    DSB_GET_MAXBAR,
    DSB_SET_MAXBAR,
    DSB_DAMAGE_POPUP,
    DSB_PPOS_CHAR,
    DSB_CHAR_PPOS,
    DSB_PPOS_TILE,
    DSB_TILE_PPOS,
    DSB_GET_XPM,
    DSB_SET_XPM,
    DSB_GIVE_XP,
    DSB_GET_XP,
    DSB_SET_XP,
    DSB_GET_TEMP_XP,
    DSB_SET_TEMP_XP,
    DSB_XP_LEVEL,
    DSB_XP_LEVEL_NB,
    DSB_GET_BONUS,
    DSB_SET_BONUS,
    DSB_MSG,
    DSB_MSG_CHAIN,
    DSB_DELAY_MSGS_TO,
    DSB_LOCK_GAME,
    DSB_UNLOCK_GAME,
    DSB_GET_GAMELOCKS,
    DSB_TILESHIFT,
    DSB_RAND,
    DSB_RAND_SEED,
    DSB_ATTACK_TEXT,
    DSB_ATTACK_DAMAGE,
    DSB_EXPORT,
    DSB_REPLACE_METHODS,
    DSB_LINESPLIT,
    DSB_GET_GAMEFLAG,
    DSB_SET_GAMEFLAG,
    DSB_CLEAR_GAMEFLAG,
    DSB_TOGGLE_GAMEFLAG,
    DSB_AI,
    DSB_AI_BOSS,
    DSB_AI_SUBORDINATES,
    DSB_AI_PROMOTE,
    DSB_GET_LIGHT,
    DSB_GET_LIGHT_TOTAL,
    DSB_SET_LIGHT,
    DSB_SET_LIGHT_TOTALMAX,
    DSB_GET_PORTRAITNAME,
    DSB_SET_PORTRAITNAME,
    DSB_REPLACE_CHARHAND,
    DSB_REPLACE_INVENTORY,
    DSB_REPLACE_TOPIMAGES,
    DSB_GET_EXVIEWINST,
    DSB_SET_EXVIEWINST,
    DSB_GET_LEADER,
    DSB_GET_CASTER,
    DSB_LOOKUP_GLOBAL,
    DSB_TEXTFORMAT,
    DSB_CHAMPION_TOPARTY,
    DSB_CHAMPION_FROMPARTY,
    DSB_PARTY_SCANFOR,
    DSB_SPAWNBURST_BEGIN,
    DSB_SPAWNBURST_END,
    DSB_FULLSCREEN,
    DSB_GET_ALT_WALLSET,
    DSB_ALT_WALLSET,
    DSB_WALLSET_OVERRIDE_FLOOR,
    DSB_WALLSET_OVERRIDE_ROOF,
    DSB_WALLSET_FLIP_FLOOR,
    DSB_WALLSET_FLIP_ROOF,
    DSB_WALLSET_ALWAYS_DRAW_FLOOR,
    DSB_GET_LASTMETHOD,
    DSB_SET_LASTMETHOD,
    DSB_IMPORT_ARCH,
    DSB_GET_PENDINGSPELL,
    DSB_SET_PENDINGSPELL,
    DSB_POLL_KEYBOARD,
    DSB_DMTEXTCONVERT,
    DSB_NUMBERTEXTCONVERT,
    ALL_LUA_FUNCS
};

int il_register_object(lua_State *LUA);
int il_nextinst(lua_State *LUA);
int il_l_tag(lua_State *LUA);
int il_obj_newidxfunc(lua_State *LUA);
int il_log(lua_State *LUA);
int il_flush(lua_State *LUA);
int il_trt_iassign(lua_State *LUA);
int il_trt_purge(lua_State *LUA);
int il_trt_obj_writeback(lua_State *LUA);
int il_activate_gui_zone(lua_State *LUA);
int il_set_internal_gui(lua_State *LUA);
int il_set_internal_shadeinfo(lua_State *LUA);

int expl_import_arch(lua_State *LUA);
int expl_add_champion(lua_State *LUA);
int expl_replace_method(lua_State *LUA);

int expl_spawn(lua_State *LUA);
int expl_fetch(lua_State *LUA);

int expl_image2map(lua_State *LUA);
int expl_text2map(lua_State *LUA);
int expl_party_place(lua_State *LUA);
int expl_use_wallset(lua_State *LUA);
int expl_level_tint(lua_State *LUA);
int expl_level_flags(lua_State *LUA);

int expl_spawnburst_begin(lua_State *LUA);
int expl_spawnburst_end(lua_State *LUA);
int expl_get_alt_wallset(lua_State *LUA);
int expl_set_alt_wallset(lua_State *LUA);

int expl_get_exviewinst(lua_State *LUA);
int expl_set_exviewinst(lua_State *LUA);

int expl_get_hp(lua_State *LUA);
int expl_set_hp(lua_State *LUA);
int expl_get_maxhp(lua_State *LUA);
int expl_set_maxhp(lua_State *LUA);
int expl_get_animtimer(lua_State *LUA);
int expl_set_animtimer(lua_State *LUA);

int expl_enable(lua_State *LUA);
int expl_disable(lua_State *LUA);
int expl_toggle(lua_State *LUA);

int expl_msg_chain(lua_State *LUA);

int expl_set_gameflag(lua_State *LUA);
int expl_clear_gameflag(lua_State *LUA);
int expl_toggle_gameflag(lua_State *LUA);

int expl_set_mflag(lua_State *LUA);
int expl_clear_mflag(lua_State *LUA);
int expl_toggle_mflag(lua_State *LUA);

int expl_get_gfxflag(lua_State *LUA);
int expl_set_gfxflag(lua_State *LUA);
int expl_clear_gfxflag(lua_State *LUA);
int expl_toggle_gfxflag(lua_State *LUA);

int expl_get_xp_multiplier(lua_State *LUA);

int expl_rand(lua_State *LUA);
int expl_rand_seed(lua_State *LUA);

int expl_get_charge(lua_State *LUA);
int expl_set_charge(lua_State *LUA);

int expl_find_arch(lua_State *LUA);
int expl_get_coords(lua_State *LUA);
int expl_get_coords_prev(lua_State *LUA);
int expl_move(lua_State *LUA);
int expl_qswap(lua_State *LUA);

int expl_get_cell(lua_State *LUA);
int expl_set_cell(lua_State *LUA);

int expl_get_facedir(lua_State *LUA);
int expl_set_facedir(lua_State *LUA);

int expl_valid_inst(lua_State *LUA);

int expl_get_crop(lua_State *LUA);
int expl_set_crop(lua_State *LUA);

int expl_forward(lua_State *LUA);
int expl_tileshift(lua_State *LUA);

int expl_linesplit(lua_State *LUA);

int expl_get_crmotion(lua_State *LUA);
int expl_set_crmotion(lua_State *LUA);

int expl_delete(lua_State *LUA);

int expl_dmtextconvert(lua_State *LUA);
int expl_numbertextconvert(lua_State *LUA);

int expl_get_xp(lua_State *LUA);
int expl_set_xp(lua_State *LUA);

int expl_tileptr_rotate(lua_State *LUA);
