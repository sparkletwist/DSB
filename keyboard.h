enum {
    KD_FORWARD,
    KD_LEFT,
    KD_BACK,
    KD_RIGHT,
    KD_LEFT_TURN,
    KD_RIGHT_TURN,
    KD_INVENTORY1,
    KD_INVENTORY2,
    KD_INVENTORY3,
    KD_INVENTORY4,
    KD_INVENTORY_CYCLE,
    KD_INVENTORY_CYCLE_BACK,
    
    KD_ROWSWAP,
    KD_SIDESWAP,
    KD_LEADER_CYCLE,
    KD_LEADER_CYCLE_BACK,
    KD_WZ00,
    KD_WZ01,
    KD_WZ02,
    KD_WZ03,
    KD_FREEZEGAME,
    KD_GOSLEEP,
    KD_QUICKSAVE,
    KD_QUICKQUIT,
    
    KD_MZ01,
    KD_MZ02,
    KD_MZ03,
    KD_MZ04,
    KD_MZ05,
    KD_MZ06,
    KD_MZ07,
    KD_MZ08,
    KD_MZ09,
    KD_MZ10,
    KD_MZ11,
    KD_MZ12,
    
    MAX_KEYDIR
};

struct dsbkbinfo {
    unsigned char want_config;
    unsigned char cV2;
    unsigned char cV3;
    unsigned char cV4;
    
    int k[MAX_KEYDIR][2];
};

void get_key_config(void);
void gui_key_config(void);
void gui_frozen_screen_menu(void);
int DSBcheckkey(int i_kbc);
int DSBcheckkey_state(int i_kbc);
