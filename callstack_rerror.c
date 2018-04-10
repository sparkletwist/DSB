#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <signal.h>
#include "objects.h"
#include "global_data.h"
#include "defs.h"
#include "uproto.h"

extern int c_STACKLEVEL;
extern char vstack[4096];
extern char *csptr;

extern struct global_data gd;
extern FILE *errorlog;

void recover_error(char *reason) {
    char ohno[8192];
    int zp = 0;
    char *iptr = vstack;
    char zstack[4096];

    while(iptr < csptr) {
        if (*iptr == '\0') zstack[zp] = '\n';
        else zstack[zp] = *iptr;
        ++zp;
        ++iptr;
    }

    zstack[zp] = '\0';

    snprintf(ohno, sizeof(ohno),
        "Error: %s\nStack Trace:\n%s\n%s%s%s%s%s",
        reason, zstack,
        "Something has happened that has caused the game to be\n",
        "left in an undefined or erroneous state. The game state\n",
        "has been written to DEBUG.DSB and more information has\n",
        "been written to LOG.TXT. They would be handy to send along\n",
        "if you're making a bug report."
    );

    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);

    fprintf(errorlog, "PROGRAM ERROR!\nReason: %s\nStack Dump:\n%s",
        reason, zstack);
    fclose(errorlog);

    save_savefile("debug.dsb", "DEBUG");

    MessageBox(win_get_window(), ohno, "oh dear", MB_ICONEXCLAMATION);
    DSBallegshutdown();
    exit(2);
}
