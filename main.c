#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <allegro.h>
#include <winalleg.h>
#include <signal.h>
#include "versioninfo.h"
#include "gameloop.h"
#include "objects.h"
#include "global_data.h"
#include "sound.h"
#include "gproto.h"
#include "uproto.h"
#include "compile.h"
#include "arch.h"
#include "gamelock.h"
#include "keyboard.h"
#include "istack.h"
#include "trans.h"

//typedef void (*sigptr)(int);

FONT *ROOT_SYSTEM_FONT;
static const char *DSBNAME = "Dungeon Strikes Back";
int cracked = 0;
extern char *Iname[4];
int windowed = 0;
int dynamicswitch = 1;

int debug = 0;
volatile unsigned int updateok = 0;
FILE *errorlog;
BITMAP *scr_pages[3];
BITMAP *soft_buffer;

HANDLE semaphore;
int T_BPS;
int CLOCKFREQ;

extern unsigned int cwsa_num;
extern struct wallset *cwsa;

struct wallset **ws_tbl;
extern struct wallset *haze_base;
int ws_tbl_len;

struct global_data gd;
struct inventory_info gii;

int show_fps;

extern struct dungeon_level *dun;
extern struct graphics_control gfxctl;
extern struct dsbkbinfo kbi;

extern CRITICAL_SECTION Queue_CS;

extern int iname_entries;

void DSBallegshutdown(void) {
    onstack("DSBallegshutdown");
    allegro_exit();

    if (gd.ini_options & INIOPT_SEMA)
        CloseHandle(semaphore);

    VOIDRETURN();
}

/*
void crap() {
    BITMAP *xb;
    BITMAP *dest;
    int x;
    int y;   
    xb = load_pcx("core_gfx/scrollfont.pcx", NULL);
    dest = create_bitmap(640, 480);
    clear_to_color(dest, makecol(255, 255, 0));

    for (y=0;y<33;++y) {
        rectfill(dest, 8+y*16, 8, 8+13+y*16, 8+11, makecol(255, 0, 255));
    }
    
    for (x=0;x<33;++x) {
        blit(xb, dest, x*14, 0, 8+x*16, 24, 14, 12);
    }    
    save_pcx("core_gfx/scrollfont2.pcx", dest, NULL);   
    exit(1);          
}
*/
//*
struct inames *g2tmpl = NULL;
static char *g2path = "auto_gfx";

int g2gen(const char *fname, int a, void *blah) {
    struct inames *st;
    char *g2xname;
    char *g2xext;
    
    onstack("g2gen");
    
    st = dsbmalloc(sizeof(struct inames));
    st->path = g2path;
    
    splitformatdup(fname, &g2xname, &g2xext);
    
    st->name = g2xname;
    st->ext = g2xext;
    
    st->n = g2tmpl;
    g2tmpl = st;
    
    RETURN(0);
}

void autogenerateg2() {
    char *pattern = "auto_gfx/*.pcx";

    onstack("autogenerateg2");
    
    for_each_file_ex(pattern, 0, 0, g2gen, NULL);
    
    compile_dsbgraphics("graphics2.dat", gd.filecount, g2tmpl);
    
    poop_out("Compiled!");
    exit(1);
}
// */
///////////////////////////

void closeprogram(void) {
    gd.stored_rcv = 255;
}

void disp_color_depth(int depth) {
    onstack("disp_color_depth");
    fprintf(errorlog,"INIT: Using color depth of %d\n", depth);
    fflush(errorlog);
    
    gd.color_depth = depth;
    VOIDRETURN();
}

// only used when using semaphore based timing!
void DSBsemaphoretimer(void) {
	updateok++;
	ReleaseSemaphore(semaphore, 1, NULL);
}
END_OF_FUNCTION(DSBsemaphoretimer);

int WINAPI WinMain (HINSTANCE h_this, HINSTANCE h_prev,
                    LPSTR argv, int CRAP)
{
    int color_depth;
    int alt_color_depth = 0;
    int i_action;
    int glv = 1;
    int my_GFXMODE = GFX_DIRECTX;
    int my_SWITCHMODE = SWITCH_AMNESIA;
    int tvar;
    int grv = 0;
    int x_mode = 0;
    int new_game = 1;
    int double_window = 0;
    int ugly_window = 0;
    int df = 2;
    const char *tmp_dprefix;
    
    signal(SIGSEGV, sigcrash);
    signal(SIGILL, sigcrash);
    signal(SIGFPE, sigcrash);
    
    init_stack();
    errorlog = fopen("log.txt", "w");
    
    onstack("DSBmain");
    
    v_onstack("DSBmain.install");
    InitializeCriticalSectionAndSpinCount(&Queue_CS, 1000);
    install_allegro(SYSTEM_NONE, &errno, atexit);

    three_finger_flag = 1;
    memset(&gd, 0, sizeof(struct global_data));
    memset(&gfxctl, 0, sizeof(struct graphics_control));
    memset(&gii, 0, sizeof(struct inventory_info));
    memset(&kbi, 0, sizeof(struct dsbkbinfo));
    memset_sound();
    init_all_ist();
    dun = NULL;
    cwsa_num = 0;
    cwsa = NULL;
    Iname[0] = Iname[1] = Iname[2] = Iname[3] = NULL;
    
    gd.max_total_light = 255;

    gd.exestate = STATE_LOADING;
    gd.p_lev[0] = gd.p_lev[1] = gd.p_lev[2] = gd.p_lev[3] = LOC_LIMBO;
    gd.who_look = -1;
    gd.leader = -1;
    gd.offering_ppos = -1;
    gd.gl = dsbcalloc(1, sizeof(struct gamelock));
    // make this the default until the base code asserts it
    gfxctl.console_lines = 4;
    gfxctl.itemname_drawzone = 0;
    
    ws_tbl = NULL;
    ws_tbl_len = 0;
	haze_base = NULL;
	
	if (CURRENT_BETA) {
        fprintf(errorlog, "@@@ DEBUG VERSION %d.%d.%d\n", MAJOR_VERSION, MINOR_VERSION, CURRENT_BETA);
    }
	v_upstack();
	
	fprintf(errorlog, "INIT: Parsing configuration file\n");
	
	if (debug || CURRENT_BETA) {
        char *tmp_dir = dsbfastalloc(500);
        GetTempPath(500, tmp_dir);
        fprintf(errorlog, "Temporary path is %s\n", tmp_dir);    
    }
    	
	set_config_file("dsb.ini");
	
	tmp_dprefix = get_config_string("Main", "Dungeon", NULL);
	if (tmp_dprefix)
        gd.dprefix = dsbstrdup(tmp_dprefix);
    else {
        if (selectdungeondir() == 0) {
            poop_out("No dungeon selected. Exiting.");
        }
    }
	
	gd.compile = get_config_int("Main", "Compile", 0);
	
	gd.bprefix = dsbstrdup(get_config_string("Settings", "Base", "base"));
	gd.trip_buffer = get_config_int("Settings", "TripleBuffer", 1);
	
	cracked = get_config_int("Settings", "Cracked", 0);
	
	windowed = get_config_int("Settings", "Windowed", 0);
	dynamicswitch = get_config_int("Settings", "ModeSwitch", 1);
	if (dynamicswitch || windowed) {
        my_GFXMODE = GFX_AUTODETECT_WINDOWED;
        my_SWITCHMODE = SWITCH_BACKGROUND;
    }
    double_window = get_config_int("Settings", "DoubleWindow", 0);
    ugly_window = get_config_int("Settings", "UglyScaleWindow", 0);
    
    gd.soundfade = get_config_int("Settings", "SoundFade", 60);
    
    tvar = get_config_int("Settings", "CumulativeDamage", 1);
    if (tvar)
        gd.ini_options |= INIOPT_CUDAM;
        
    tvar = get_config_int("Settings", "LeaderVision", 0);
    if (tvar)
        gd.ini_options |= INIOPT_LEADERVISION;
        
    tvar = get_config_int("Settings", "SemaphoreTiming", 1);
    if (tvar)
        gd.ini_options |= INIOPT_SEMA;
        
    tvar = get_config_int("Settings", "AtariPointer", 0);
    if (tvar)
        gd.ini_options |= INIOPT_STMOUSE;
    
	show_fps = get_config_int("Settings", "ShowFPS", 0);
	
	T_BPS = get_config_int("Settings", "GameBPS", 35);
	if (T_BPS < 3)
        T_BPS = 3;
    else if (T_BPS > 100)
        T_BPS = 100;
    if (T_BPS != 35) {
        if (gd.ini_options & INIOPT_SEMA) {
            MessageBox(win_get_window(), "Your GameBPS is not 35.\nSemaphoreTiming will be disabled, and DSB may have other issues.",
                "Error", MB_ICONEXCLAMATION);
            gd.ini_options &= ~(INIOPT_SEMA);
        } else {
            MessageBox(win_get_window(), "Your GameBPS is not 35.\nDSB may break in strange ways.",
                "Warning", MB_ICONEXCLAMATION);
        }
    }
	
	CLOCKFREQ = get_config_int("Settings", "ClockDivide", 7);
    if (CLOCKFREQ < 2)
        CLOCKFREQ = 2;
    if (CLOCKFREQ >= T_BPS)
        CLOCKFREQ = T_BPS - 1;
	
	debug = get_config_int("Settings", "Debug", 0);
	if (debug)
        show_fps = 1;
	
	if (!strcasecmp(gd.dprefix, gd.bprefix))
        gd.uprefix = 1;
    else
        gd.uprefix = 0;
        
    fprintf(errorlog, "INIT: Using gamedir %s (%s:%d)\n", gd.dprefix, gd.bprefix, gd.uprefix);

	color_depth = get_config_int("Settings", "ColorDepth", 32);
	if (color_depth == 16) alt_color_depth = 15;
	if (color_depth == 32) alt_color_depth = 32;

	if (!alt_color_depth) {
        poop_out("Invalid ColorDepth\nMust be 16 or 32");
    }
    
    get_key_config();
    
    allegro_exit();
    if (x_mode) {
        //grv = EditorMain(h_this, h_prev, argv, CRAP);
        goto SHUTDOWN_EVERYTHING;
    } else {
        allegro_init();
        install_keyboard();
	    install_timer();
        install_mouse();
    }
    
    set_close_button_callback(closeprogram);
    
    if (windowed)
        fprintf(errorlog, "INIT: Starting windowed mode\n"); 
    else   
        fprintf(errorlog, "INIT: Starting fullscreen mode\n");
        
    if (windowed) {
        if (double_window) {
            gd.double_size = 1;
            gd.trip_buffer = 0;
            df = 4;
            dynamicswitch = 0;
        } else if (ugly_window) {
            gd.double_size = 3;
            gd.trip_buffer = 0;
            df = 3;
            dynamicswitch = 0;
        }
    }
    	
    request_refresh_rate(70);
	set_color_depth(color_depth);
	if (set_gfx_mode(my_GFXMODE, (XRES*df)/2, (YRES*df)/2, 0, 0) < 0) {
        color_depth = alt_color_depth;
		set_color_depth(color_depth);
			if (set_gfx_mode(my_GFXMODE, (XRES*df)/2, (YRES*df)/2, 0, 0) < 0) {
                poop_out(allegro_error);
			} else
				disp_color_depth(alt_color_depth);
	} else
	   disp_color_depth(color_depth);

	init_colordepth(color_depth);
	set_color_conversion(COLORCONV_KEEP_ALPHA);
    
	fflush(errorlog);

    fsound_init();

	set_display_switch_mode(my_SWITCHMODE);
	init_systemtimer();
	
    if (gd.ini_options & INIOPT_SEMA) {
        semaphore = CreateSemaphore(NULL, 0, 1, NULL);
        install_int_ex(DSBsemaphoretimer, BPS_TO_TIMER(T_BPS));
    }

	if (gd.trip_buffer && (gfx_capabilities & GFX_CAN_TRIPLE_BUFFER)) {
        fprintf(errorlog, "INIT: Setting up triple buffer\n");
        fflush(errorlog);
    	scr_pages[0] = create_video_bitmap(XRES, YRES);
    	scr_pages[1] = create_video_bitmap(XRES, YRES);
    	scr_pages[2] = create_video_bitmap(XRES, YRES);
    	clear_to_color(scr_pages[0], makecol(0,0,72));
    	gd.scr_blit = 0;
    	gd.scr_show = 0;
    	request_video_bitmap(scr_pages[gd.scr_show]);
    } else {
        gd.trip_buffer = 0;
        fprintf(errorlog, "INIT: Triple buffering unavailable or disabled.\n");
        clear_to_color(screen, makecol(0,0,72));
        scr_pages[0] = screen;
    }
    
    if (!windowed && dynamicswitch) {
        if (set_gfx_mode(GFX_DIRECTX, XRES, YRES, 0, 0) < 0) {
            poop_out(allegro_error);
        }
        
        set_display_switch_mode(SWITCH_AMNESIA);
        if (!gd.trip_buffer)
            scr_pages[0] = screen; 
        clear_to_color(scr_pages[0], makecol(0,0,72));
        if (gd.trip_buffer)
            request_video_bitmap(scr_pages[0]);
    }
    
    //autogenerateg2();
    init_fonts();
    psxf("INITIALIZING SYSTEM");
    init_console();
    initmovequeue();
    blank_translation_table();
    purge_dirty_list();
    set_config_file("dsb.ini");
    set_keyboard_rate(0, 0);
    
    psxf("ALLOCATING BACKBUFFER");
    soft_buffer = create_bitmap(XRES, YRES);

    psxf("INITIALIZING TIMERS");
    init_t_events();
    
    psxf("INITIALIZING RNG");
    gd.session_seed = clock() + time(NULL);
    srand_to_session_seed();
    
    psxf("INITIALIZING LUA");
    initialize_lua();
    // this also loads all custom scripts
    
    psxf("IMPORTING LUA DATA");
	import_lua_constants();
	
	psxf("IMPORTING SYSTEM GRAPHICS");
    get_sysgfx();
    
    psxf("IMPORTING CONDITIONS");
    register_lua_conditions();
    
    gd.exestate = STATE_OBJPARSE;
    psxf("IMPORTING OBJECTS");
    initialize_objects();
    load_system_objects(LOAD_OBJ_ALL);
    purge_object_translation_table();
    
    gd.exestate = STATE_DUNGEONLOAD;     
    psxf("LOADING DUNGEON");   
    load_dungeon("DUNGEON_FILENAME", "COMPILED_DUNGEON_FILENAME");

    if (gd.compile && iname_entries) {
        psxf("SAVING GRAPHICS.DSB");
        write_dsbgraphics();
    }
    
    gd.exestate = STATE_ISTARTUP;
    psxf("STARTING INTERFACE");
    import_interface_constants();
    init_interface_click();
    import_shading_info();
    import_door_draw_info();
    
    // turn off compilation
    gd.compile = 0;
    if (kbi.want_config) {
        gd.exestate = STATE_KEYCONFIG;
        gui_key_config();
        
        if (gd.stored_rcv == 255)
            goto SHUTDOWN_GAME;
    }
    show_introduction();
    
    reget_lua_font_handle();
    
    gd.exestate = STATE_FRONTDOOR;
    gd.glowmask = NULL;
    i_action = show_front_door();
    if (i_action > 0) {
        gd.exestate = STATE_RESUME;
        show_load_ready(0);
        new_game = 0;
        if (gd.stored_rcv == 255)
            glv = 255;
    } else if (i_action < 0)
        glv = 255;
        
    gd.exestate = STATE_GAMEPLAY;
    fflush(errorlog);
    
    while (glv < 100) {
        int no_sound_halt = 0;
        
        glv = DSBgameloop(new_game);
        fflush(errorlog);
        
        halt_all_sound_channels();

        if (glv == -2) {
            glv = show_the_end_screen(0);
            new_game = 0;
            no_sound_halt = 1;
        } else if (glv == -3) {
            glv = show_ending(gd.ending_boring);
        } else if (glv == -4) {
            load_export_target();
            new_game = 1;  
        } else
            break;
            
        if (!no_sound_halt)
            halt_all_sound_channels();
    }
    
    SHUTDOWN_GAME:
    gd.exestate = STATE_QUITTING;
    if (gd.trip_buffer) {
        int n;
        for (n=0;n<3;++n) {
            clear_to_color(scr_pages[n], makecol(0,0,72));
            request_video_bitmap(scr_pages[n]);
        }
    } else
        clear_to_color(screen, makecol(0,0,72));
    
    destroy_dungeon();
    destroy_animap(gd.simg.mouse);
    destroy_animap(gd.simg.mousehand);
    destroy_lod_list();
    
    SHUTDOWN_EVERYTHING:
    DSBallegshutdown();
    DeleteCriticalSection(&Queue_CS);
    
	fprintf(errorlog, "SHUTDOWN: Shutting down...\n");
	fclose(errorlog);
	
    dsbfree(gd.gl);
	
	return grv;
}

