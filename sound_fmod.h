struct channel_entry {
    char id[16];
    FMOD_SOUND *sample;
    FMOD_CHANNEL *chan;
    int flags;
};

struct channel_list {
    int max;
    struct channel_entry *c;
};

struct frozen_chan {
    int i;
    char uid[16];
    FMOD_SOUND *sample;
    float vol;
    int cmode;
    int cpos;
    int x;
    int y;
    int z;
    int XXX_1;
    int XXX_2;
    struct frozen_chan *n;
};

FMOD_SOUND *soundload(const char *sname, const char *longname, int comp, int quietfail, int use_3d);
FMOD_SOUND *luasound(lua_State *LUA, int stackarg, char *id, char *fname, int paramnum);
int play_3dsound(FMOD_SOUND *fs, const char *id, int lev, int xx, int yy, int);
int play_ssound(FMOD_SOUND *fs, const char *id, int, int use3d);

FMOD_SOUND *internal_load_sound_data(char *s_dataname);
FMOD_SOUND *do_load_music(const char *musicname, char *id, int force);

char *hash_and_cache(lua_State *LUA, FMOD_SOUND *lua_sound, const char *name);
void store_in_master_by_id(const char *unique_id, FMOD_SOUND *lua_sound);

