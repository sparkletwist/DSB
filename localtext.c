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
#include "htext.h"
#include "localtext.h"

//typedef void (*sigptr)(int);

struct system_locstr syslocstr;
extern struct global_data gd;

void process_localtable_in_lua(Htextbuf *loc_buf) {
    Htext *cl = loc_buf->tl;
    
    onstack("process_localtable_in_lua");
    
    while(cl) {
        char *base_ptr = cl->t;
        
        if (*base_ptr != '\0') {
            while(*base_ptr == ' ' || *base_ptr == '\t') {
                base_ptr++;
            }
            
            if (*base_ptr != '\0') {
                char *k = base_ptr;
                char *v = strchr(base_ptr, '\t');
                if (!v) {
                    v = strchr(base_ptr, ' ');
                }
                
                if (v) {
                    *v = '\0';
                    v++;
                    
                    while(*v == '\t') v++;
                    
                    if (v != '\0') {
                        int si = 0;
                        int di = 0;
                        char v_modified[300];
                    
                        while (v[si] != '\0') {
                            if (v[si] == '\\') {
                                int digit;
                                char conv[4] = { 0, 0, 0, 0 };
                                char nchar = '?';
                                
                                si++;
                                if (v[si] == '\\') {
                                    nchar = '\\';
                                    si++;
                                } else {
                                    for(digit=0;digit<3;digit++) {
                                        if (isdigit(v[si])) {
                                            conv[digit] = v[si];
                                            si++;
                                        } else
                                            break;
                                    }
                                    if (conv[0] != '\0') {
                                        nchar = atoi(conv);
                                    }
                                }
                                
                                v_modified[di] = nchar;
                                di++;
                            } else {
                                v_modified[di] = v[si];
                                si++;
                                di++;
                            }
                            
                            if (di == 299)
                                break;
                        }
                        v_modified[di] = '\0';
                        
                        lua_localization_table_entry(k, v_modified);
                    }
                }    
            }
        }
        
        cl = cl->n;   
    }
    
    VOIDRETURN();
}

void process_localtext_file_in_lua(const char *fb) {
    Htextbuf *loc_buf;
    
    onstack("process_localtext_file_in_lua");
    
    loc_buf = read_and_break(0, fb, -1);
    if (loc_buf) {
        process_localtable_in_lua(loc_buf);
        destroy_textbuf(loc_buf);   
    }
    
    VOIDRETURN();
}

void import_and_setup_localization() {
    const char *localtext = "localtext";
    const char *localtext_ext = ".txt";
    char localtext_base[20];
    char localtext_local[20];
    
    onstack("import_and_setup_localization");
    
    sprintf(localtext_base, "%s%s", localtext, localtext_ext);
    sprintf(localtext_local, "%s%s%c%c%s", localtext, "_", gd.locale[0], gd.locale[1], localtext_ext);
    
    begin_localization_table();
    process_localtext_file_in_lua(bfixname(localtext_base));
    process_localtext_file_in_lua(bfixname(localtext_local));
    process_localtext_file_in_lua(fixname(localtext_base));
    process_localtext_file_in_lua(fixname(localtext_local));
    end_localization_table();
    
    syslocstr.dsbmenu = get_lua_localtext("SYSTEM_DSBMENU", "PRESS ENTER FOR DSB MENU");
    syslocstr.gamefrozen = get_lua_localtext("SYSTEM_GAMEFROZEN", "GAME FROZEN");
    syslocstr.wakeup = get_lua_localtext("SYSTEM_WAKEUP", "WAKE UP");
    syslocstr.quicksavefail = get_lua_localtext("ERROR_QUICKSAVE", "ALL SAVE SLOTS ARE FULL. PLEASE SAVE MANUALLY.");
    syslocstr.cantsave = get_lua_localtext("DISALLOW_SAVE", "SAVING NOT ALLOWED.");
    syslocstr.cantsleep = get_lua_localtext("DISALLOW_SLEEP", "SLEEPING NOT ALLOWED.");
    
    VOIDRETURN();
}
