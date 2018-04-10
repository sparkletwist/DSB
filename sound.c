#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <fmod.h>
#include <fmod_errors.h>
#include "filedefs.h"
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "sound.h"
#include "sound_fmod.h"

#define MAX_VOICES 40

static FMOD_SOUND *interface_click_sound;

FMOD_SYSTEM *f_system;

extern FILE *errorlog;
extern struct global_data gd;
extern struct dungeon_level *dun;

extern const char *DSB_MasterSoundTable;

struct channel_list fchans;
extern lua_State *LUA;

void fmod_errcheck(FMOD_RESULT result) {
    if (result != FMOD_OK) {
        char errstr[300];
        snprintf(errstr, sizeof(errstr), "FMOD error %d\n%s\n", result,
            FMOD_ErrorString(result));
        program_puke(errstr);
    } 
}

void fmod_uerrcheck(FMOD_RESULT result) {
    if (result != FMOD_OK) {
        char errstr[30];
        snprintf(errstr, sizeof(errstr), "FMOD update error %d", result);
        program_puke(errstr);
    }
}

int add_to_channeltable(FMOD_SOUND *samp,
    FMOD_CHANNEL *chan, const char *id_str) 
{
    struct channel_entry *c_chan;
    int i;
    int next = -1;
    
    onstack("add_to_channeltable");
    
    for(i=0;i<fchans.max;++i) {
        if (fchans.c[i].flags == CHAN_AVAIL) {
            next = i;
            c_chan = &(fchans.c[next]);
            break;
        }
    }
    
    if (next == -1) {
        next = fchans.max;
        fchans.c = dsbrealloc(fchans.c,
            sizeof(struct channel_entry) * (next+1));
        fchans.max++;
        c_chan = &(fchans.c[next]);
    }
    
    c_chan->flags = CHAN_INUSE;
    c_chan->chan = chan;
    c_chan->sample = samp;
    memcpy(c_chan->id, id_str, 16);
    
    RETURN(next);
}

void check_sound_channels(void) {
    int highest = 0;
    int i;
    
    onstack("check_sound_channels");
    
    for(i=0;i<fchans.max;++i) {
        if (fchans.c[i].chan != NULL) {
            int pchk;
            FMOD_RESULT result;
            int stolen = 0;
            
            result = FMOD_Channel_IsPlaying(fchans.c[i].chan, &pchk);
            if (result == FMOD_ERR_CHANNEL_STOLEN ||
                result == FMOD_ERR_INVALID_HANDLE)
            {
                stolen = 1;
            } else
                fmod_errcheck(result);

            if (pchk && !stolen) {
                highest = i;
            } else {
                if (!stolen) {
                    result = FMOD_Channel_Stop(fchans.c[i].chan);
                    fmod_errcheck(result);
                }

                fchans.c[i].chan = NULL;
                fchans.c[i].sample = NULL;
                fchans.c[i].flags = CHAN_AVAIL;
            }
        }
    }
        
    // get rid of music handle if it's ended
    v_onstack("check_sound_channels.music_check");
    for(i=0;i<MUSIC_HANDLES;++i) {
        if (gd.cur_mus[i].uid) {
            int gdc = gd.cur_mus[i].chan;
            int chanstate = fchans.c[gdc].flags;
            if (chanstate == CHAN_AVAIL) {               
                current_music_ended(i);
            }
        }
    }
    v_upstack();
    
    // realloc smaller if we don't need stuff
    // and doing it isn't going to mess with some saved settings!
    if (!gd.frozen_sounds) {
        if (highest < fchans.max) {
            fchans.max = highest + 1;
            fchans.c = dsbrealloc(fchans.c,
                sizeof(struct channel_entry) * fchans.max);
        }
    }
    
    VOIDRETURN();
}

int check_sound(unsigned int cid) {
    FMOD_RESULT result;
    int pchk;

    onstack("check_sound");

    if (cid >= fchans.max)
        RETURN(0);

    if (fchans.c[cid].flags == CHAN_AVAIL)
        RETURN(0);
        
    result = FMOD_Channel_IsPlaying(fchans.c[cid].chan, &pchk);
    if (result == FMOD_ERR_CHANNEL_STOLEN ||
        result == FMOD_ERR_INVALID_HANDLE)
    {
        pchk = 0;
    } else
        fmod_errcheck(result);

    if (pchk) {
        RETURN(1);
    }

    fchans.c[cid].chan = NULL;
    fchans.c[cid].sample = NULL;
    fchans.c[cid].flags = CHAN_AVAIL;

    RETURN(0);
}

void stop_sound(unsigned int cid) {
    FMOD_RESULT result;
    int pchk;

    onstack("stop_sound");
    
    if (cid >= fchans.max)
        VOIDRETURN();
        
    if (fchans.c[cid].flags == CHAN_AVAIL)
        VOIDRETURN();
        
    result = FMOD_Channel_IsPlaying(fchans.c[cid].chan, &pchk);
    fmod_errcheck(result);
    if (pchk) {
        result = FMOD_Channel_Stop(fchans.c[cid].chan);
        fmod_errcheck(result);
    }
    
    fchans.c[cid].chan = NULL;
    fchans.c[cid].sample = NULL;
    fchans.c[cid].flags = CHAN_AVAIL;
        
    VOIDRETURN();
}

void destroy_all_sound_handles(void) {
    struct frozen_chan *fz;
    int i;
    
    onstack("destroy_all_sound_handles");
    
    fz = gd.frozen_sounds;
    while (fz) {
        struct frozen_chan *fz_n = fz->n;
        condfree(fz);
        fz = fz_n;
    }
    gd.frozen_sounds = NULL;
    
    for(i=0;i<fchans.max;++i) {
        if (fchans.c[i].chan != NULL) {
            int pchk;
            FMOD_Channel_IsPlaying(fchans.c[i].chan, &pchk);

            if (pchk) {
                int result;
                result = FMOD_Channel_Stop(fchans.c[i].chan);
                fmod_errcheck(result);
            }
        }
    }
    
    condfree(fchans.c);
    memset_sound();
    
    VOIDRETURN();
}

void destroy_all_music_handles(void) {
    int i;
    
    onstack("destroy_all_music_handles");
    
    for(i=0;i<MUSIC_HANDLES;++i) { 
        if (gd.cur_mus[i].uid) {
            dsbfree(gd.cur_mus[i].uid);
            dsbfree(gd.cur_mus[i].filename);
            memset(&(gd.cur_mus[i]), 0, sizeof(struct current_music));
        }     
    }
    
    VOIDRETURN();
}

void store_frozen_channel(int i, const char *uid, FMOD_SOUND *sample,
    float vol, int cmode, int cpos, int x, int y, int z)
{
    struct frozen_chan *n_fz = dsbmalloc(sizeof(struct frozen_chan));
    
    n_fz->i = i;
    memcpy(n_fz->uid, uid, 16);
    n_fz->sample = sample;
    n_fz->vol = vol;
    n_fz->cmode = cmode;
    n_fz->cpos = cpos;
    n_fz->x = x;
    n_fz->y = y;
    n_fz->z = z;
    n_fz->n = gd.frozen_sounds;
    
    gd.frozen_sounds = n_fz;
}

void freeze_sound_channels(void) {
    FMOD_RESULT result;
    int i;
    
    onstack("freeze_sound_channels");
    
    check_sound_channels();
    
    for(i=0;i<fchans.max;++i) {                    
        if (fchans.c[i].flags == CHAN_INUSE) {
            int pchk;
            struct channel_entry *cc = &(fchans.c[i]);
                       
            FMOD_Channel_IsPlaying(cc->chan, &pchk);
                        
            if (pchk) {
                int x, y, z;
                int cmode, cpos;
                float vol;
                FMOD_Channel_GetMode(cc->chan, &cmode);
                FMOD_Channel_GetPosition(cc->chan, &cpos, FMOD_TIMEUNIT_MS);
                FMOD_Channel_GetVolume(cc->chan, &vol);
            
                if (cmode & FMOD_3D_WORLDRELATIVE) {
                    FMOD_VECTOR fvec, nvec;
                    
                    FMOD_Channel_Get3DAttributes(cc->chan, &fvec, &nvec);
                    
                    x = fvec.x;
                    y = fvec.y;
                    z = fvec.z;
                }
                
                store_frozen_channel(i, cc->id, cc->sample, vol,
                    cmode, cpos, x, y, z);
                
                FMOD_Channel_Stop(cc->chan);
            }
            
            cc->chan = NULL;
            cc->sample = NULL;
            cc->flags = CHAN_AVAIL;
        }
    }
    
    for(i=0;i<MUSIC_HANDLES;++i) {
        gd.cur_mus[i].preserve = 1;
    }
    
    VOIDRETURN();
}

void halt_all_sound_channels(void) {
    FMOD_RESULT result;
    int i;
    
    onstack("halt_all_sound_channels");
    
    check_sound_channels();
    
    for(i=0;i<fchans.max;++i) {                    
        if (fchans.c[i].flags == CHAN_INUSE) {
            int pchk;
            struct channel_entry *cc = &(fchans.c[i]);                       
            FMOD_Channel_IsPlaying(cc->chan, &pchk);                        
            if (pchk) {
                FMOD_Channel_Stop(cc->chan);
            }
            cc->chan = NULL;
            cc->sample = NULL;
            cc->flags = CHAN_AVAIL;
        }
    }
    for(i=0;i<MUSIC_HANDLES;++i) {
        gd.cur_mus[i].preserve = 1;
    }
    
    VOIDRETURN();    
}

void unfreeze_sound_channels(void) {
    struct frozen_chan *fz;
    FMOD_RESULT result;

    onstack("unfreeze_sound_channels");
    
    fz = gd.frozen_sounds;
    while (fz) {
        int im;
        struct frozen_chan *fz_n = fz->n;
        
        struct channel_entry *cc = &(fchans.c[fz->i]);
        
        FMOD_VECTOR nulv = {  0.0f, 0.0f, 0.0f };
        FMOD_CHANNEL *channel;
        int result;
                
        //* DEBUG */ fprintf(errorlog, "Playing Sound\n");
        result = FMOD_System_PlaySound(f_system, FMOD_CHANNEL_FREE, fz->sample,
            TRUE, &channel);
        fmod_errcheck(result);
        
        //* DEBUG */ fprintf(errorlog, "Unfreezing sound [%x] to index %d\n", fz->sample, fz->i);
        
        //* DEBUG */ fprintf(errorlog, "Setting ModePosition\n");
        FMOD_Channel_SetMode(channel, fz->cmode);
        FMOD_Channel_SetPosition(channel, fz->cpos, FMOD_TIMEUNIT_MS);
        FMOD_Channel_SetVolume(channel, fz->vol);
                
        //* DEBUG */ fprintf(errorlog, "Setting Vector\n");
        if (fz->cmode & FMOD_3D_WORLDRELATIVE) {
            FMOD_VECTOR fvec;
            fvec.x = fz->x;
            fvec.y = fz->y;
            fvec.z = fz->z;
            FMOD_Channel_Set3DAttributes(channel, &fvec, &nulv);
        } else if (fz->cmode & FMOD_3D_HEADRELATIVE) {
            FMOD_Channel_Set3DAttributes(channel, &nulv, &nulv);
        } 
            
        result = FMOD_Channel_SetPaused(channel, FALSE);
        fmod_errcheck(result);  
        
        for(im=0;im<MUSIC_HANDLES;++im) {
            if (gd.cur_mus[im].uid && gd.cur_mus[im].preserve) {
                if (!strncmp(fz->uid, gd.cur_mus[im].uid, 16)) {
                    gd.cur_mus[im].chan = fz->i;
                    //* DEBUG */ fprintf(errorlog, "Index %d is music %s\n", fz->i, fz->uid);
                }
            }
            gd.cur_mus[im].preserve = 0;
        }
                    
        memcpy(cc->id, fz->uid, 16);
        cc->sample = fz->sample;
        cc->chan = channel;

        cc->flags = CHAN_INUSE;
        
        condfree(fz);
        fz = fz_n;    
    }
    
    /*
    if (!gd.frozen_sounds)
        fprintf(errorlog, "No frozen sounds!\n");
    */
         
    gd.frozen_sounds = NULL;
    
    VOIDRETURN();
}

void memset_sound(void) {
    onstack("memset_sound");
    memset(&fchans, 0, sizeof(struct channel_list));
    VOIDRETURN();
}

FMOD_SOUND *soundload(const char *sname, const char *longname,
    int compilation, int quietfail, int is_not_music) 
{
    int compiled = 0;
    DATAFILE *rdat = NULL;
    char s_str[256];
    const char *noslash_sname;
    FMOD_RESULT res;
    FMOD_SOUND *nsnd;
    FMOD_CREATESOUNDEXINFO ex_i;
    int flags_3d;
    
    onstack("soundload");
    stackbackc('(');
    v_onstack(sname);
    addstackc(')');
    
    if (is_not_music)
        flags_3d = FMOD_3D;
    else
        flags_3d = FMOD_2D;
        
    noslash_sname = make_noslash(sname);
    
    if (gd.compile && compilation)
        compiled = 1;
        
    if (longname)
        snprintf(s_str, sizeof(s_str), "%s%s%s", gd.dprefix, "/", longname);
    
    if (!longname)
        snprintf(s_str, sizeof(s_str), "%s%s%s%s", gd.dprefix, "/", sname, ".wav");
    res = FMOD_System_CreateSound(f_system, s_str, flags_3d, NULL, &nsnd);
    if (res == FMOD_OK) {
        if (compiled)
            add_iname(sname, noslash_sname, "wav", longname);
        RETURN(nsnd);
    }
        
    if (!longname)
        snprintf(s_str, sizeof(s_str), "%s%s%s%s", gd.dprefix, "/", sname, ".mp3");
    res = FMOD_System_CreateSound(f_system, s_str,
        flags_3d | FMOD_CREATECOMPRESSEDSAMPLE, NULL, &nsnd);
    if (res == FMOD_OK) {
        if (compiled)
            add_iname(sname, noslash_sname, "mp3", longname);
        RETURN(nsnd);
    }
    
    if (!longname)
        snprintf(s_str, sizeof(s_str), "%s%s%s%s", gd.dprefix, "/", sname, ".ogg");
    res = FMOD_System_CreateSound(f_system, s_str,
        flags_3d | FMOD_CREATECOMPRESSEDSAMPLE, NULL, &nsnd);
    if (res == FMOD_OK) {
        if (compiled)
            add_iname(sname, noslash_sname, "ogg", longname);
        RETURN(nsnd);
    }
    
    if (!longname)
        snprintf(s_str, sizeof(s_str), "%s%s%s%s", gd.dprefix, "/", sname, ".mod");
    res = FMOD_System_CreateSound(f_system, s_str, flags_3d, NULL, &nsnd);
    if (res == FMOD_OK) {
        if (compiled)
            add_iname(sname, noslash_sname, "mod", longname);
        RETURN(nsnd);
    }
    
    if (!longname)
        snprintf(s_str, sizeof(s_str), "%s%s%s%s", gd.dprefix, "/", sname, ".mid");
    res = FMOD_System_CreateSound(f_system, s_str, flags_3d, NULL, &nsnd);
    if (res == FMOD_OK) {
        if (compiled)
            add_iname(sname, noslash_sname, "mid", longname);
        RETURN(nsnd);
    }

    rdat = load_datafile_object(fixname("graphics.dsb"), noslash_sname);
    
    if (rdat == NULL)
        rdat = load_datafile_object(bfixname("graphics.dat"), noslash_sname);    
    
    if (rdat == NULL)
        goto SOUNDLOAD_ERROR;
    
    memset(&ex_i, 0, sizeof(FMOD_CREATESOUNDEXINFO));
    ex_i.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    ex_i.length = rdat->size; 
       
    res = FMOD_System_CreateSound(f_system, rdat->dat,
        FMOD_OPENMEMORY | FMOD_CREATECOMPRESSEDSAMPLE | flags_3d, 
        &ex_i, &nsnd); 
            
    unload_datafile_object(rdat);
    
    if (res == FMOD_OK) {
        RETURN(nsnd);
    } else {
        fmod_errcheck(res);
        RETURN(NULL);
    }
                
    SOUNDLOAD_ERROR:
    if (!quietfail) {
        snprintf(s_str, sizeof(s_str), "Sound not found:\n%s", sname);
        poop_out(s_str);  
    }  
    RETURN(NULL);
}

void sound_3d_settings(int atten) {
    FMOD_RESULT result;
    
    onstack("sound_3d_settings");
    
    double f_atten;
    f_atten = (double)atten / 100.0;
    
    result = FMOD_System_Set3DSettings(f_system, 1.0, 0.5, f_atten);
    fmod_errcheck(result);
    
    VOIDRETURN();
}

void fsound_init(void) {
    FMOD_RESULT result;
    
    onstack("fsound_init");
    
    fprintf(errorlog, "INIT: Starting FMOD\n");
    
    result = FMOD_System_Create(&f_system);
    fmod_errcheck(result);
    
    result = FMOD_System_Init(f_system, MAX_VOICES,
        FMOD_INIT_3D_RIGHTHANDED, NULL);
    fmod_errcheck(result);
    
    sound_3d_settings(gd.soundfade);
    
    VOIDRETURN();
}

void coord2vector(FMOD_VECTOR *fv, int lev, int x, int y) {
    struct dungeon_level *dd;
    
    dd = &(dun[lev]);

    fv->x = x;
    fv->y = 1000*lev;
    fv->z = dd->ysiz - y;
}    

void set_3d_soundcoords(void) {
    FMOD_VECTOR up_v = { 0.0f, -1.0f, 0.0f };
    FMOD_VECTOR forward_v = { 0.0f, 0.0f, 0.0f };
    FMOD_VECTOR my_v;
    int dx, dy;
    int ap = gd.a_party;
    
    onstack("set_3d_soundcoords");
    
    memset(&my_v, 0, sizeof(FMOD_VECTOR));
    coord2vector(&my_v, gd.p_lev[ap], gd.p_x[ap], gd.p_y[ap]);
    
    face2delta(gd.p_face[ap], &dx, &dy);
    forward_v.x = (float)dx;
    forward_v.z = (float)(-1 * dy);
    
    FMOD_System_Set3DListenerAttributes(f_system, 0,
        &my_v, NULL, &forward_v, &up_v);
    fmod_update();
        
    VOIDRETURN();    
}

int play_3dsound(FMOD_SOUND *fs, const char *id, int lev, int xx, int yy, int loop) {
    FMOD_VECTOR my_v;
    FMOD_VECTOR vel = {  0.0f, 0.0f, 0.0f };
    FMOD_RESULT result;
    FMOD_CHANNEL *channel;
    int sh;
    int lflag = 0;
    
    onstack("play_3dsound");
    
    memset(&my_v, 0, sizeof(FMOD_VECTOR));
    coord2vector(&my_v, lev, xx, yy);
    
    if (loop)
        lflag = FMOD_LOOP_NORMAL;
    
    result = FMOD_System_PlaySound(f_system, FMOD_CHANNEL_FREE, fs, TRUE, &channel); 
    fmod_errcheck(result);
    result = FMOD_Channel_SetVolume(channel, 1.0f);
    fmod_errcheck(result);
    FMOD_Channel_SetMode(channel, FMOD_3D_WORLDRELATIVE | lflag);
    FMOD_Channel_Set3DAttributes(channel, &my_v, &vel);  
    result = FMOD_Channel_SetPaused(channel, FALSE);
    fmod_errcheck(result);
    
    sh = add_to_channeltable(fs, channel, id);
    
    RETURN(sh);
}

int play_ssound(FMOD_SOUND *fs, const char *id, int loop, int use_3d) {
    FMOD_VECTOR v = {  0.0f, 0.0f, 0.0f };
    FMOD_RESULT result;
    FMOD_CHANNEL *channel;
    int sh;
    int lflag = 0;
    
    onstack("play_ssound");
    
    if (loop)
        lflag = FMOD_LOOP_NORMAL;
    
    result = FMOD_System_PlaySound(f_system, FMOD_CHANNEL_FREE, fs, TRUE, &channel); 
    fmod_errcheck(result);
    result = FMOD_Channel_SetVolume(channel, 1.0f);
    fmod_errcheck(result);
    
    if (use_3d) {
        result = FMOD_Channel_SetMode(channel, FMOD_3D_HEADRELATIVE | lflag);
        fmod_errcheck(result); 
        result = FMOD_Channel_Set3DAttributes(channel, &v, &v);
        fmod_errcheck(result);
    } else {
        result = FMOD_Channel_SetMode(channel, FMOD_2D | lflag);
        fmod_errcheck(result);  
    }       
    
    result = FMOD_Channel_SetPaused(channel, FALSE);
    fmod_errcheck(result);
    
    sh = add_to_channeltable(fs, channel, id);
    
    RETURN(sh);
}

void fmod_update(void) {
    FMOD_RESULT result;
    
    onstack("fmod_update");
    
    result = FMOD_System_Update(f_system);
    fmod_uerrcheck(result);
    
    VOIDRETURN();
}

void read_all_sounddata(PACKFILE *pf) {
    int ns;

    onstack("read_all_sounddata");

    if (fchans.max != 0)
        program_puke("fchans already exists");
        
    rdl(fchans.max);
    
    if (fchans.max) {
        lua_getglobal(LUA, DSB_MasterSoundTable);       
        
        fchans.c = dsbcalloc(fchans.max, sizeof(struct channel_entry));
        
        rdl(ns);
        //fprintf(errorlog, "SOUND: Loading item %d of %d\n", ns, fchans.max);        
        while (ns != KEY_STRING) {
            char f_uid[16];
            FMOD_SOUND *rsample = NULL;
            int v;
            int cmode, cpos;
            float vol;
            int x, y, z;
                        
            //* DEBUG */ fprintf(errorlog, "Reading ID\n");
            for(v=0;v<16;++v) {
                rdc(f_uid[v]);
            }

            //* DEBUG */ fprintf(errorlog, "Reading Modes\n");
            rdl(cmode);
            rdl(cpos);
            pack_fread(&vol, sizeof(float), pf);
            
            if (cmode & FMOD_3D_WORLDRELATIVE) {
                rdl(x);
                rdl(y);
                rdl(z);
            } 

            lua_pushstring(LUA, f_uid);
            lua_gettable(LUA, -2);

            if (lua_isnil(LUA, -1)) {
                fprintf(errorlog, "ERROR: Retrieved nil from MasterSoundTable [%s]\n", f_uid);
            } else
                rsample = (FMOD_SOUND *)lua_touserdata(LUA, -1);
            
            if (rsample != NULL) {
                FMOD_VECTOR nulv = {  0.0f, 0.0f, 0.0f };
                
                store_frozen_channel(ns, f_uid, rsample,
                    vol, cmode, cpos, x, y, z);                

            } else {
                fprintf(errorlog, "SOUND: Hash %s on channel %d FAILED\n",
                    f_uid, ns);    
                global_stack_dump_request();               
                poop_out("Saved sound not found\n\nSee log.txt for details");
            }
            lua_pop(LUA, 1);

            rdl(ns);
        }
        
        lua_pop(LUA, 1);
    }

    VOIDRETURN();
}

void write_all_sounddata(PACKFILE *pf) {
    struct frozen_chan *fz;
    
    onstack("write_all_sounddata");
    
    if (gd.frozen_sounds) {
        freeze_sound_channels();
    }     
    wrl(fchans.max);
    
    fz = gd.frozen_sounds; 
    
    while (fz) {
        int v;
        wrl(fz->i);
        
        for(v=0;v<16;++v) {
            wrc(fz->uid[v]);
        }
        
        wrl(fz->cmode);
        wrl(fz->cpos);
        pack_fwrite(&(fz->vol), sizeof(float), pf);
        
        if (fz->cmode & FMOD_3D_WORLDRELATIVE) {
            wrl(fz->x);
            wrl(fz->y);
            wrl(fz->z);
        }
        
        fz = fz->n;
    }
    
    if (fchans.max)
        wrl(KEY_STRING);
    
    VOIDRETURN();
}

void sound_destroy_and_dc(const char *id) {
    FMOD_SOUND *dc_sound;
    
    onstack("sound_destroy_and_dc");
    
    stackbackc('(');
    v_onstack(id);
    addstackc(')');    
    
    luastacksize(4);
        
    lua_getglobal(LUA, DSB_MasterSoundTable);
    
    lua_pushstring(LUA, id);
    lua_gettable(LUA, -2);
    dc_sound = (FMOD_SOUND *)lua_touserdata(LUA, -1);
    FMOD_Sound_Release(dc_sound);
    lua_pop(LUA, 1);
    
    lua_pushstring(LUA, id);
    lua_pushnil(LUA);
    lua_settable(LUA, -3);    
    // pop the table itself
    lua_pop(LUA, 1);
 
    VOIDRETURN();        
}

void interface_click(void) {
    FMOD_RESULT result;

    onstack("interface_click");

    result = FMOD_System_PlaySound(f_system, FMOD_CHANNEL_FREE,
        interface_click_sound, FALSE, NULL);
    fmod_errcheck(result);

    VOIDRETURN();
}

void init_interface_click(void) {
    onstack("init_interface_click");
    interface_click_sound = internal_load_sound_data("CLICK");
    VOIDRETURN();
}

int get_sound_vol(unsigned int shand) {
    struct channel_entry *cc;
    int vol;
    float fvol;
    
    onstack("get_sound_vol");
    
    if (shand >= fchans.max)
        RETURN(-1);
        
    cc = &(fchans.c[shand]);

    if (cc->flags == CHAN_AVAIL)
        RETURN(-1);

    FMOD_Channel_GetVolume(cc->chan, &fvol);
    vol = fvol * 100;
    
    RETURN(vol);
}

void set_sound_vol(unsigned int shand, unsigned int vol) {
    struct channel_entry *cc;
    float fvol;

    onstack("set_sound_vol");

    if (shand >= fchans.max)
        VOIDRETURN();

    cc = &(fchans.c[shand]);

    if (cc->flags == CHAN_AVAIL)
        VOIDRETURN();

    fvol = (float)vol / 100.0f;
    FMOD_Channel_SetVolume(cc->chan, fvol);
    
    VOIDRETURN();
}

FMOD_SOUND *do_load_music(const char *musicname, 
    char *unique_id, int force_loading) 
{
    int quietfail;
    char *tmp_id;
    FMOD_SOUND *lua_music;
    
    onstack("do_load_music");
    
    if (!force_loading) {
        int im;
        for(im=0;im<MUSIC_HANDLES;++im) {
            if (gd.cur_mus[im].filename != NULL) {
                if (!strcmp(musicname, gd.cur_mus[im].filename)) {
                    memcpy(unique_id, gd.cur_mus[im].uid, 16);
                    RETURN(NULL);
                }
            }
        }    
    }
    
    /*** OLD COMMENT:
    // this works for now. make it an extra parameter if
    // this function gets called more than the two places
    // it is called now...
    ******/
    // except, it already IS an extra parameter.
    // I don't know what I'm doing here, anymore.
    quietfail = force_loading;
    
    lua_music = soundload(musicname, NULL, 0, quietfail, 0);
    tmp_id = hash_and_cache(LUA, lua_music, musicname);
    memcpy(unique_id, tmp_id, 16);
    
    RETURN(lua_music);
}

void current_music_ended(int music_chan) {
    onstack("current_music_ended");
    
    if (gd.cur_mus[music_chan].uid) {
        if (!gd.cur_mus[music_chan].preserve) {
            sound_destroy_and_dc(gd.cur_mus[music_chan].uid);
            dsbfree(gd.cur_mus[music_chan].uid);
            dsbfree(gd.cur_mus[music_chan].filename);
            gd.cur_mus[music_chan].uid = NULL;
            gd.cur_mus[music_chan].filename = NULL;
            gd.cur_mus[music_chan].preserve = 0;
        }
    }
    
    VOIDRETURN();
}

