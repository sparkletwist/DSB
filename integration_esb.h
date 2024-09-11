struct esb_ipc_data {
    HWND dsb_hwnd;
    unsigned int ipc_msg;
    
    HANDLE shared_memory;
    integration_data *sdata;
    
    char dsb_running;
    char __UNUSED_2;
    char __UNUSED_3;
    char __UNUSED_4;
};

int dsb_is_running(void);

void ed_clear_integration_structure(void);
void ed_start_test_in_dsb(void);
void ed_reset_dsb(void);

void force_shutdown_dsb(void);
void ed_integration_shutdown(void);
void dsb_communication(WPARAM wp, LPARAM lp);

void integration_party_moved(int lev, int x, int y, int dir);
void integration_update_obj_parameters(unsigned int id, int b_send_update);
void integration_send_exvars_to_dsb(unsigned int id);
void integration_dungeon_resized(void);

void integration_arch_change(unsigned int inst, const char *newname);
void integration_inst_update(unsigned int inst);
void integration_tileposition_update(unsigned int inst, int tiledir);
 
char *get_integration_exvar(unsigned int id);
