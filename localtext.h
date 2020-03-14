struct system_locstr {
    char *gamefrozen;
    char *dsbmenu;
    char *wakeup;
    char *quicksavefail;
    char *cantsave;
    char *cantsleep;
};

void begin_localization_table(void);
void end_localization_table(void);
void lua_localization_table_entry(const char *k, const char *v);

char *get_lua_localtext(const char *key, const char *defval);
