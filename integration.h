typedef struct _integration_data {
    int l_lev;
    int l_x;
    int l_y;
    int l_dir;
    unsigned int l_id;
    unsigned int l_id_result;
    int l_newdir;
    char l_arch[128];
    char stored_str[8192];  
} integration_data;

enum {
    INTEGRATION_NONE,
    INTEGRATION_DSB_HWND_AND_SHARED_MEMORY,
    INTEGRATION_ESB_HWND,
    INTEGRATION_FORCE_QUIT,
    INTEGRATION_DSB_SHUTDOWN,
    INTEGRATION___UNUSED,
    INTEGRATION_GAME_RESET,
    INTEGRATION_DUNGEON_RESIZE_WARNING,
    
    INTEGRATION_OBJECT_ID_CHECK = 20,
    
    INTEGRATION_LUA_COMMAND = 50,
    INTEGRATION_LOCATION_CHANGE,
    INTEGRATION_INSTTILEDIR_CHANGE,
    
    INTEGRATION_OBJECT_SPAWN_LUA = 100,
    INTEGRATION_OBJECT_DELETE_LUA,
};

unsigned int get_integration_inst(void);
void finalize_integration_obj_add(unsigned int id, const char *arch_name, int lev, int x, int y, int dir);
void integration_change_cell(int lev, int x, int y, int nval);
void integration_inst_destroy(unsigned int inst, struct obj_arch *p_arch);

void begin_integration_cs(void);
void end_integration_cs(void);

void begin_console_cs(void);
void end_console_cs(void);
