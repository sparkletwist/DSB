#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include "versioninfo.h"
#include "gameloop.h"
#include "objects.h"
#include "global_data.h"
#include "gproto.h"
#include "uproto.h"
#include "arch.h"
#include "integration.h"
#include "integration_dsb.h"

CRITICAL_SECTION Integration_CS;
CRITICAL_SECTION Console_CS;

struct dsb_ipc_data dsb_ipc;
extern struct global_data gd;
extern struct inst *oinst[NUM_INST];
extern char inst_dirty[NUM_INST];

extern FILE *errorlog;

extern int debug;

WNDPROC allegro_wndproc;

char *dsb_testing_dungeon_filename = NULL;

void dsb_setup_esb_test_mode(void) {
    onstack("dsb_setup_esb_test_mode");
    
    memset(&dsb_ipc, 0, sizeof(struct dsb_ipc_data));  
    
    dsb_ipc.ipc_msg = RegisterWindowMessageA("DSBESBCommunication");
    if (dsb_ipc.ipc_msg == 0) {
        poop_out("Could not register DSB control message");
        VOIDRETURN();
    }
    gd.testing_mode = TESTMODE_STARTING;
    VOIDRETURN();
}

void esb_sync_lost(const char *why) {
    int alertcol;
    char alertmsg[200];
    
    alertcol = makecol(255,160,0);
    
    sprintf(alertmsg, "SYNC LOST: %s", why);
    console_system_message(alertmsg, alertcol);
    console_system_message("PLEASE \"RESET DSB\" FROM THE ESB MENU.", alertcol);   
}

// THIS IS NOT CALlED VIA THE DSB THREAD, IT MUST BE THREAD SAFE
// AND IT CANNOT TOUCH THE CALLSTACK
void esb_communication(WPARAM msgtype, LPARAM val) {
    switch (msgtype) {  
        case INTEGRATION_ESB_HWND: {
            dsb_ipc.esb_hwnd = (HWND)val;
        } break;

        case INTEGRATION_FORCE_QUIT: {
            dsb_ipc.esb_hwnd = 0;
            gd.stored_rcv = 255;
        } break;
        
        case INTEGRATION_GAME_RESET: {
            gd.everyone_dead = 2;
        } break;
                 
        case INTEGRATION_LUA_COMMAND: {
            //fprintf(errorlog, "INTEGRATION COMMAND: [%s]\n", dsb_ipc.sdata->stored_str);
            lua_command_to_iq(dsb_ipc.sdata->stored_str);
        } break;

        case INTEGRATION_OBJECT_ID_CHECK: {
            if (oinst[dsb_ipc.sdata->l_id]) {
                unsigned int reserve_inst = next_object_inst_nocallstack(1, 0xFFFFFFFF, dsb_ipc.sdata->l_id);
                gd.esb_reserved_inst = reserve_inst;
                dsb_ipc.sdata->l_id_result = reserve_inst;
            } else {
                dsb_ipc.sdata->l_id_result = dsb_ipc.sdata->l_id;
                gd.esb_reserved_inst = dsb_ipc.sdata->l_id;
            }
        } break;

        case INTEGRATION_OBJECT_SPAWN_LUA: {
            unsigned int sinst = dsb_ipc.sdata->l_id;
            int obj_spawn_ok = 1;
            
            if (oinst[sinst]) {
                char alertmsg[60];
                
                obj_spawn_ok = 0;
                
                if (oinst[sinst]->level == LOC_LIMBO) {
                    struct obj_arch *p_arch = Arch(oinst[sinst]->arch);
                    if (!strcmp(p_arch->luaname, dsb_ipc.sdata->l_arch)) {
                        // just a limbo version of its previous self, overwrite it
                        char delete_cmd[90];
                        sprintf(delete_cmd, "dsb_delete(%lu)\n", sinst);
                        lua_command_to_iq(delete_cmd);
                        obj_spawn_ok = 1;
                    }  
                }
                
                if (!obj_spawn_ok) {
                    sprintf(alertmsg, "INST %lu IN USE", sinst);
                    esb_sync_lost(alertmsg);
                }
            }
            
            if (obj_spawn_ok) {
                lua_command_to_iq(dsb_ipc.sdata->stored_str);
            }
        } break;
        
        case INTEGRATION_OBJECT_DELETE_LUA: {
            unsigned int sinst = dsb_ipc.sdata->l_id;
            int lost_sync_deletion = 0;
            int already_gone = 0;
            
            // already gone, nothing for us to do!
            if (!oinst[sinst]) {
                already_gone = 1;
            } else {
                struct obj_arch *p_arch = Arch(oinst[sinst]->arch);
                if (strcmp(p_arch->luaname, dsb_ipc.sdata->l_arch)) {
                    lost_sync_deletion = 1;
                }   
            }
            
            if (lost_sync_deletion) {
                char alertmsg[60];
                sprintf(alertmsg, "INST %d MISMATCH", sinst);
                esb_sync_lost(alertmsg);
            } else if (!already_gone) {
                lua_command_to_iq(dsb_ipc.sdata->stored_str);
                
                if (debug) {
                    char debugmessage[60];
                    sprintf(debugmessage, "ESB DELETED INST %u", sinst);
                    console_system_message(debugmessage, makecol(255,255,255));
                }
            }
    
        } break;
        
        case INTEGRATION_INSTTILEDIR_CHANGE: {
            unsigned int r_inst = dsb_ipc.sdata->l_id;
            if (!oinst[r_inst]) {
                char syncmsg[60];
                sprintf(syncmsg, "INST %lu DESTROYED", r_inst);
                esb_sync_lost(syncmsg);    
            } else {
                struct inst *p_inst = oinst[r_inst];
                if (p_inst->tile != dsb_ipc.sdata->l_newdir) {
                    char luacmd[128];
                    sprintf(luacmd, "dsb_move(%u, %d, %d, %d, %d)\n",
                        r_inst, p_inst->level, p_inst->x, p_inst->y, dsb_ipc.sdata->l_newdir);
                    lua_command_to_iq(luacmd);
                }   
            }            
            
        } break;
 
        case INTEGRATION_DUNGEON_RESIZE_WARNING: {
            esb_sync_lost("DUNGEON RESIZED");
        } break;
    }
}

void begin_integration_cs() {
    EnterCriticalSection(&Integration_CS);   
}

void end_integration_cs() {
    LeaveCriticalSection(&Integration_CS);     
}

void begin_console_cs() {
    EnterCriticalSection(&Console_CS);   
}

void end_console_cs() {
    LeaveCriticalSection(&Console_CS);     
}

LRESULT CALLBACK dsb_testmode_wndproc(HWND allegro_wnd, UINT message, WPARAM wparam, LPARAM lparam) {

    if (message == dsb_ipc.ipc_msg) {
        // there are more elegant ways to do this... but that's too much work
        // just lock every ESB command inside the critical section
        EnterCriticalSection(&Integration_CS);
        esb_communication(wparam, lparam);
        LeaveCriticalSection(&Integration_CS); 
    }

    return CallWindowProc(allegro_wndproc, allegro_wnd, message, wparam, lparam);  
}

// CALLED FROM MAIN THREAD
void announce_testing_window(void) {
    HWND dsb_window = win_get_window();
    onstack("announce_testing_window");
    
    if (!dsb_window) {
        poop_out("Could not get DSB window!");
        VOIDRETURN();
    }
    
    // create shared memory
    dsb_ipc.shared_memory = CreateFileMapping(INVALID_HANDLE_VALUE,    // use paging file
                 NULL,                    // default security
                 PAGE_READWRITE,          // read/write access
                 0,                       // maximum object size (high-order DWORD)
                 sizeof(integration_data),                // maximum object size (low-order DWORD)
                 "DSBESBMappingObject");                 // name of mapping object
                 
    if (dsb_ipc.shared_memory == NULL) {
        char errstr[200];
        sprintf(errstr, "Could not create shared memory object! (Error %d)", GetLastError());
        poop_out(errstr);
        VOIDRETURN();
    }
    
    dsb_ipc.sdata = (integration_data*)MapViewOfFile(dsb_ipc.shared_memory, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(integration_data));
    if (dsb_ipc.sdata == NULL) {
        poop_out("Could not get pointer to shared data!");
        CloseHandle(dsb_ipc.shared_memory);
        VOIDRETURN();        
    }
    
    fprintf(errorlog, "DSB shared memory [%x %x]\n", dsb_ipc.shared_memory, dsb_ipc.sdata);
    
    gd.testing_mode = TESTMODE_ACTIVE;
    
    // hook the allegro wndproc and do our own stuff first
    allegro_wndproc = (WNDPROC)GetWindowLong(dsb_window, GWL_WNDPROC);
    SetWindowLong(dsb_window, GWL_WNDPROC, (long)dsb_testmode_wndproc);
        
    SendMessage(HWND_BROADCAST, dsb_ipc.ipc_msg, INTEGRATION_DSB_HWND_AND_SHARED_MEMORY, (LPARAM)dsb_window);
    VOIDRETURN();
}

void integration_party_moveto(int lev, int x, int y, int dir) {
    onstack("integration_party_moveto");
    if (gd.testing_mode == TESTMODE_ACTIVE) {
        dsb_ipc.sdata->l_lev = lev;
        dsb_ipc.sdata->l_x = x;
        dsb_ipc.sdata->l_y = y;
        dsb_ipc.sdata->l_dir = dir;
        SendMessage(dsb_ipc.esb_hwnd, dsb_ipc.ipc_msg, INTEGRATION_LOCATION_CHANGE, 0);  
    }
    VOIDRETURN();
}

void testing_dungeon_filename(const char *dungeon_filename) {
    onstack("testing_dungeon_filename");

    dsbfree(dsb_testing_dungeon_filename);
    dsb_testing_dungeon_filename = dsbstrdup(dungeon_filename);
    
    VOIDRETURN();   
}

void testing_reload(void) {
    int push_exestate = gd.exestate;
    
    onstack("testing_reload");
    
    gd.exestate = STATE_DUNGEONLOAD;
    destroy_dungeon();
    load_dungeon("DUNGEON_FILENAME", "COMPILED_DUNGEON_FILENAME");
    DeleteFile(fixname(dsb_testing_dungeon_filename));
    gd.exestate = push_exestate;
    
    VOIDRETURN();   
}

void notify_esb_and_clear_shared_testing_memory(void) {
    if (gd.testing_mode >= TESTMODE_ACTIVE) {
        if (dsb_ipc.esb_hwnd != 0) {
            PostMessage(dsb_ipc.esb_hwnd, dsb_ipc.ipc_msg, INTEGRATION_DSB_SHUTDOWN, 0);
        }
        
        UnmapViewOfFile((PVOID)dsb_ipc.sdata);
        CloseHandle(dsb_ipc.shared_memory);
    }
}

void lua_command_to_iq(const char *luacmd) {
    integration_queue *niq;
    
    niq = dsbcalloc(1, sizeof(integration_queue));
    
    niq->cmd = DSBINTEGRATION_EXEC_LUA;
    niq->lua = dsbstrdup_nocallstack(luacmd); 
    
    if (dsb_ipc.iq == NULL) {
        dsb_ipc.iq = niq;  
    } else {
        dsb_ipc.iq_current->n = niq;     
    }
    dsb_ipc.iq_current = niq; 
}

void flush_integration_queue(void) {
    onstack("flush_integration_queue");
    while (dsb_ipc.iq != NULL) {
        integration_queue *iq = dsb_ipc.iq;
        integration_queue *iq_n = iq->n;
        
        if (iq->lua) {
            lc_dostring(iq->lua);
            dsbfree(iq->lua);
        }
        dsbfree(iq);
        
        dsb_ipc.iq = iq_n;
    }
    dsb_ipc.iq_current = NULL;
    VOIDRETURN();
}

void integration_fail() {
    program_puke("Bad integration request"); 
}

unsigned int get_integration_inst(void) {
    onstack("get_integration_inst");    
    integration_fail();
    RETURN(0);
}

void finalize_integration_obj_add(unsigned int id, const char *arch_name, int lev, int x, int y, int dir) {
    onstack("finalize_integration_obj_add");    
    integration_fail();
    VOIDRETURN();
}

void integration_change_cell(int lev, int x, int y, int nval) {
    onstack("integration_change_cell");    
    integration_fail();
    VOIDRETURN();    
}

void integration_inst_destroy(unsigned int inst, struct obj_arch *p_arch) {
    onstack("integration_inst_destroy");    
    integration_fail();
    VOIDRETURN();      
}
