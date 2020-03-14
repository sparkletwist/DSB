#define MAX_SOUND_POLYGON   16384

enum {
    CHAN_AVAIL,
    CHAN_INUSE
};

void fsound_init(void);
void set_3d_soundcoords(void);
void fmod_update(void);

int setup_level_sound_geometry(int lev);

int check_sound(unsigned int cid);
void stop_sound(unsigned int cid);
void stop_all_sound_channels(void);

void read_all_sounddata(PACKFILE *pf);
void write_all_sounddata(PACKFILE *pf);

int get_sound_vol(unsigned int shand);
void set_sound_vol(unsigned int shand, unsigned int vol);
void current_music_ended(int cmu);

void import_sound_info(void);
