enum {
    CHAN_AVAIL,
    CHAN_INUSE
};

void fsound_init(void);
void set_3d_soundcoords(void);
void fmod_update(void);

int check_sound(unsigned int cid);
void stop_sound(unsigned int cid);

void read_all_sounddata(PACKFILE *pf);
void write_all_sounddata(PACKFILE *pf);

int get_sound_vol(unsigned int shand);
void set_sound_vol(unsigned int shand, unsigned int vol);
void current_music_ended(int cmu);
