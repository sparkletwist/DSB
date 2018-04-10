#include <allegro.h>
#include <winalleg.h>

static unsigned long long int perfreq;
static unsigned long long int m_freq;
LARGE_INTEGER per_v;
LARGE_INTEGER old_v;

extern int T_BPS;

void init_systemtimer(void) {
    LARGE_INTEGER p_freq;
    QueryPerformanceFrequency(&p_freq);
    perfreq = p_freq.QuadPart / T_BPS;
    m_freq = p_freq.QuadPart / 1000;
    
    per_v.QuadPart = 0;
    old_v.QuadPart = 0;
}

int getsleeptime(void) {
    unsigned long long int d;
    LARGE_INTEGER c_v;
    
    QueryPerformanceCounter(&c_v);

    if (c_v.QuadPart >= old_v.QuadPart + perfreq)
        return 0;
        
    d = (old_v.QuadPart + perfreq) - c_v.QuadPart;
 
    return ((d/m_freq) - 1);
}

int systemtimer(void) {
    
    QueryPerformanceCounter(&per_v);
    
    if (per_v.QuadPart > old_v.QuadPart + perfreq) {
        int ticks = (per_v.QuadPart - old_v.QuadPart)/perfreq;
        old_v.QuadPart = per_v.QuadPart;
        
        return ticks;
    }
    
    return 0;
}
