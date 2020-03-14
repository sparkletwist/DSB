typedef struct _Htext {
    int linelen;
    char *t;
    struct _Htext *n;
} Htext;

typedef struct _Htextbuf {
    int numlines;
    Htext *tl;
} Htextbuf;

Htextbuf *read_and_break(int directread, const char *filename, int maxlinelen);
void destroy_textbuf(Htextbuf *d);
