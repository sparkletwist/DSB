struct defdraw {
    BITMAP *bmp;
    int destroy;
    int sm;
    int tx;
    int ty;
    int cf;

    unsigned char flip;
    unsigned char alpha;
    unsigned char glow;
    unsigned char c_v3;
    
    struct inst *p_t_inst;
    
    struct defdraw *n;    
};

int render_shaded_object(int sm, BITMAP *scx, BITMAP *sub_b,
    int mx, int my, int tf, int flip, int tws, int alpha,
    struct inst *t_inst, int glow);
    
int defer_draw(int type, BITMAP *sub_b, int used_sub,
    int sm, int mx, int my, int cf, int flip_it, int alpha,
    struct inst *t_inst, int glow);
    
void render_deferred(BITMAP *scx);
