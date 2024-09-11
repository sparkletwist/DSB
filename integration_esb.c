#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "E_res.h"
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "editor.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "monster.h"
#include "integration.h"
#include "integration_esb.h"

#define INTEGRATION_START(N) char *IB; if(!esb_ipc.sdata) { return; }\
    onstack(N);\
    IB = esb_ipc.sdata->stored_str

#define INTEGRATION_FINISH() SendMessage(esb_ipc.dsb_hwnd, esb_ipc.ipc_msg, INTEGRATION_LUA_COMMAND, 0);\
    VOIDRETURN()

#define INTEGRATION_MESSAGE(C) SendMessageTimeout(esb_ipc.dsb_hwnd, esb_ipc.ipc_msg, C, 0, SMTO_BLOCK, 5000, NULL)
#define INTEGRATION_MESSAGE_PARM(C, P) SendMessage(esb_ipc.dsb_hwnd, esb_ipc.ipc_msg, C, P, SMTO_BLOCK, 5000, NULL))

unsigned int esb_ipc_msg = 0;
struct esb_ipc_data esb_ipc;
extern struct inst *oinst[NUM_INST];

extern struct editor_global edg;
extern struct global_data gd;

extern HWND sys_hwnd;
extern FILE *errorlog;

int dsb_is_running(void) {
    onstack("dsb_is_running");
    
    if (esb_ipc.dsb_running && esb_ipc.sdata != NULL) {
        RETURN(1);
    }
    
    RETURN(0); 
}

int check_dsb_not_running(void) {
    onstack("check_dsb_not_running");
    if (!esb_ipc.dsb_running || esb_ipc.sdata == NULL) {
        MessageBox(NULL, "DSB is not running", "Oops", MB_ICONINFORMATION);
        RETURN(1);  
    }     
    RETURN(0);
}

void ed_clear_integration_structure(void) {
    memset(&esb_ipc, 0, sizeof(struct esb_ipc_data));   
}

void ed_save_testing_dungeon(void) {
    char exsave[MAX_PATH];
    
    onstack("ed_save_testing_dungeon");
    
    sprintf(exsave, "%s%s", edg.valid_savename, ".testing");
    editor_export_dungeon(exsave, 1);    

    VOIDRETURN();
}

void ed_start_test_in_dsb(void) {
    
    onstack("ed_start_test_in_dsb");
    
    if (esb_ipc.dsb_running) {
        MessageBox(NULL, "DSB is already running", "Oops", MB_ICONINFORMATION);
        VOIDRETURN();  
    }
    
    if (edg.valid_savename) {
        int i;
        int lastbreak = 0;
        char *valid_savename_dir;
        char *valid_savename_file;
        char cmdline[MAX_PATH];
               
        ed_save_testing_dungeon();
        
        if (esb_ipc.ipc_msg == 0) {
            esb_ipc.ipc_msg = RegisterWindowMessageA("DSBESBCommunication");
            if (esb_ipc.ipc_msg == 0) {
                MessageBox(NULL, "Could not register ESB control message", "Error", MB_ICONSTOP);
                VOIDRETURN();
            }
            esb_ipc_msg = esb_ipc.ipc_msg;
        }
        
        valid_savename_dir = dsbstrdup(edg.valid_savename);
        for(i=0;valid_savename_dir[i] != '\0';i++) {
            if (valid_savename_dir[i] == '/' || valid_savename_dir[i] == '\\') {
                lastbreak = i;
            }
        }
        if (!lastbreak) {
            MessageBox(NULL, "Bad dungeon filename", "Error", MB_ICONSTOP);
            dsbfree(valid_savename_dir);
            VOIDRETURN();   
        }   
        valid_savename_dir[lastbreak] = '\0';
        valid_savename_file = &(valid_savename_dir[lastbreak+1]);
        
        sprintf(cmdline, "-e%s%s%s", valid_savename_dir, "::", valid_savename_file);
        ShellExecute(sys_hwnd, "open", "DSB.exe", cmdline, NULL, SW_SHOW);
        Sleep(500); // give it time to open
        
        dsbfree(valid_savename_dir);
        
        esb_ipc.dsb_running = 1;
        
    } else {
        MessageBox(NULL, "Save dungeon first", "Oops", MB_ICONINFORMATION);
    }
    
    VOIDRETURN();
}

void ed_reset_dsb(void) {     
    onstack("ed_reset_dsb");
    
    if (check_dsb_not_running()) {
        VOIDRETURN(); 
    } 
    
    ed_save_testing_dungeon();
    SendMessage(esb_ipc.dsb_hwnd, esb_ipc.ipc_msg, INTEGRATION_GAME_RESET, 0);
       
    VOIDRETURN();
}

void force_shutdown_dsb(void) {
    if (esb_ipc.dsb_running) {
        SendMessage(esb_ipc.dsb_hwnd, esb_ipc.ipc_msg, INTEGRATION_FORCE_QUIT, 0);
        ed_integration_shutdown();
    }
}

void dsb_communication(WPARAM msgtype, LPARAM val) {
    onstack("dsb_communication");
    
    switch (msgtype) {    
        case INTEGRATION_DSB_HWND_AND_SHARED_MEMORY: {
            esb_ipc.dsb_hwnd = (HWND)val;
            PostMessage(esb_ipc.dsb_hwnd, esb_ipc.ipc_msg, INTEGRATION_ESB_HWND, (LPARAM)sys_hwnd);

            esb_ipc.shared_memory = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "DSBESBMappingObject");
            if (esb_ipc.shared_memory == NULL) {
                force_shutdown_dsb();
                MessageBox(NULL, "Could not open file mapping object from DSB", "Error", MB_ICONSTOP);
                VOIDRETURN(); 
            }

            esb_ipc.sdata = (integration_data*)MapViewOfFile(esb_ipc.shared_memory, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(integration_data));
            if (esb_ipc.sdata == NULL) {
                poop_out("Could not get pointer to shared data!");
                CloseHandle(esb_ipc.shared_memory);
                esb_ipc.shared_memory = NULL;
                VOIDRETURN();        
            }
            
            fprintf(errorlog, "ESB shared memory [%x %x]\n", esb_ipc.shared_memory, esb_ipc.sdata);
            
        } break;
        
        case INTEGRATION_DSB_SHUTDOWN: {
            ed_integration_shutdown();
        } break;
        
        case INTEGRATION_LOCATION_CHANGE: {
            gd.p_lev[1] = esb_ipc.sdata->l_lev;
            gd.p_x[1] = esb_ipc.sdata->l_x;
            gd.p_y[1] = esb_ipc.sdata->l_y;
            gd.p_face[1] = esb_ipc.sdata->l_dir;
            
            if (edg.draw_mode == DRM_PARTYCURRENT) {
                if (edg.ed_lev == gd.p_lev[1]) {
                    force_redraw(edg.subwin, 1); 
                }  
            }
            
        } break;
    }
    
    VOIDRETURN();
}

void ed_integration_shutdown(void) {
    esb_ipc.dsb_running = 0; 
        
    if (esb_ipc.sdata != NULL) {
        UnmapViewOfFile((PVOID)esb_ipc.sdata);
        esb_ipc.sdata = NULL;
    }
    
    if (esb_ipc.shared_memory != NULL) {
        CloseHandle(esb_ipc.shared_memory);
        esb_ipc.shared_memory = NULL;
    }    
}

void integration_party_moved(int lev, int x, int y, int dir) {
    INTEGRATION_START("integration_party_moved");
    sprintf(IB, "dsb_party_place(%d, %d, %d, %d)", lev, x, y, dir);        
    INTEGRATION_FINISH();
}

// don't use objarch, DSB doesn't know anything about it
unsigned int get_integration_inst(void) {
    unsigned int id, newid;
    
    onstack("get_integration_inst");
    
    id = next_object_inst(1, 0xFFFFFFFF, 0);
    esb_ipc.sdata->l_id = id;
    INTEGRATION_MESSAGE(INTEGRATION_OBJECT_ID_CHECK);
    newid = esb_ipc.sdata->l_id_result;
    
    if (newid != id) {
        while(oinst[newid]) {
            newid++;
            if (newid >= NUM_INST) {
                poop_out("Inst allocation failure");
                VOIDRETURN();
            }
            esb_ipc.sdata->l_id = newid;
            INTEGRATION_MESSAGE(INTEGRATION_OBJECT_ID_CHECK);
            newid = esb_ipc.sdata->l_id_result;   
        }
    }
    
    RETURN(newid);
}

void finalize_integration_obj_add(unsigned int id, const char *arch_name, int lev, int x, int y, int dir) {
    INTEGRATION_START("finalize_integration_obj_add");  
    esb_ipc.sdata->l_id = id;
    sprintf(esb_ipc.sdata->l_arch, "%s", arch_name);
    sprintf(IB, "dsb_spawn(%u, [==[%s]==], %d, %d, %d, %d)\n", id, arch_name, lev, x, y, dir);  
    INTEGRATION_MESSAGE(INTEGRATION_OBJECT_SPAWN_LUA);
    VOIDRETURN();
}

void integration_update_obj_parameters(unsigned int id, int b_send_update) {
    struct inst *p_inst;
    INTEGRATION_START("integration_update_obj_parameters");
    
    p_inst = oinst[id];
    
    if (b_send_update || p_inst->ai || p_inst->gfxflags || p_inst->facedir || p_inst->charge || p_inst->crop || p_inst->frame) {
        char monster_message[300];
        const char *disablestr;
        
        if (p_inst->gfxflags & OF_INACTIVE)
            disablestr = "true";
        else
            disablestr = "false";
            
        if (p_inst->ai && p_inst->ai->d_hp) {
            sprintf(monster_message, "mon_hp(%ld, %ld)\n", id, p_inst->ai->hp);
        } else {
            sprintf(monster_message, "\n");
        }
            
        sprintf(IB, "dsb_objset(%lu, true, %s, %ld, %ld, %ld, %ld, %lu)\n%s\n", id, disablestr,
            p_inst->gfxflags & ~(OF_INACTIVE),
            p_inst->facedir, p_inst->charge, p_inst->crop, p_inst->frame, monster_message);
            
        INTEGRATION_MESSAGE(INTEGRATION_LUA_COMMAND); 
    }
    
    VOIDRETURN();
}
        
void integration_send_exvars_to_dsb(unsigned int id) {
    INTEGRATION_START("integration_send_exvars_to_dsb");
    sprintf(IB, "exvar[%u] = %s\n", id, get_integration_exvar(id));
    INTEGRATION_FINISH();  
}

void integration_change_cell(int lev, int x, int y, int nval) {
    INTEGRATION_START("integration_change_cell");
    sprintf(IB, "dsb_set_cell(%d, %d, %d, %d)\n", lev, x, y, nval);
    INTEGRATION_FINISH();
}

void integration_inst_destroy(unsigned int inst, struct obj_arch *p_arch) {
    INTEGRATION_START("integration_inst_destroy");
    esb_ipc.sdata->l_id = inst;
    sprintf(esb_ipc.sdata->l_arch, "%s", p_arch->luaname);
    sprintf(IB, "dsb_delete(%u)\n", inst);
    INTEGRATION_MESSAGE(INTEGRATION_OBJECT_DELETE_LUA); 
    VOIDRETURN(); 
}

void integration_arch_change(unsigned int inst, const char *newname) {
    INTEGRATION_START("integration_arch_change");
    sprintf(IB, "dsb_qswap(%lu, [==[%s]==])\n", inst, newname);
    INTEGRATION_FINISH();    
}

void integration_tileposition_update(unsigned int inst, int tiledir) {
    INTEGRATION_START("integration_tileposition_update");
    esb_ipc.sdata->l_id = inst;
    esb_ipc.sdata->l_newdir = tiledir;
    INTEGRATION_MESSAGE(INTEGRATION_INSTTILEDIR_CHANGE);
    VOIDRETURN();     
}

void integration_inst_update(unsigned int inst) {
    onstack("integration_inst_update");
    
    if (!esb_ipc.sdata) {
        VOIDRETURN();
    }
    
    integration_update_obj_parameters(inst, 1);
    integration_send_exvars_to_dsb(inst);
    
    VOIDRETURN();
}

void integration_dungeon_resized(void) {
    INTEGRATION_START("integration_dungeon_resized");
    INTEGRATION_MESSAGE(INTEGRATION_DUNGEON_RESIZE_WARNING);   
    VOIDRETURN(); 
}

// critical section isn't used by ESB
void begin_integration_cs() {
}

void end_integration_cs() {
}
