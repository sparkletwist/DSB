void clipboard_mark(int tl, int tx, int ty);
void deselect_edit_clipboard(void);
void destroy_edit_clipboard(void); 

void ed_clipboard_command(int cmd);
void note_exvar_duplication(int from_id, int to_id);

enum {
    ECCM_CUT,
    ECCM_COPY,
    ECCM_PASTE
};
