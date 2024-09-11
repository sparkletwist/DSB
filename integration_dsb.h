enum {
    DSBINTEGRATION_EXEC_LUA
};

typedef struct _integration_queue {
    int cmd;
    char *lua;
    struct _integration_queue *n;
} integration_queue;

struct dsb_ipc_data {
    HWND esb_hwnd;
    HANDLE shared_memory;
    unsigned int ipc_msg;

    integration_data *sdata;
    
    integration_queue *iq;
    integration_queue *iq_current;
};

enum {
    TESTMODE_OFF,
    TESTMODE_STARTING,
    TESTMODE_ACTIVE,
    TESTMODE_GAME_RESTART
};

void dsb_setup_esb_test_mode(void);
void announce_testing_window(void);
void notify_esb_and_clear_shared_testing_memory(void);
void testing_dungeon_filename(const char *dungeon_filename);

void integration_party_moveto(int lev, int x, int y, int dir); 

void lua_command_to_iq(const char *luacmd);

void flush_integration_queue(void);
void testing_reload(void);
