#include <allegro.h>

/*
unsigned long darkdun24(unsigned long x, unsigned long y, unsigned long n) 
{
    static unsigned char *r, *g, *b, *z;

    b = (unsigned char *)&y;
    g = ((unsigned char *)&y)+1;
    r = ((unsigned char *)&y)+2;
    z = (unsigned char*)&n;

    if (*r > *z) *r -= *z; else *r = 0;
    if (*g > *z) *g -= *z; else *g = 0; 
    if (*b > *z) *b -= *z; else *b = 0;
     
    return y;    
}*/

// fast version
unsigned long nocyan_darkdun24
    (unsigned long x, unsigned long y, unsigned long n)
{
    register int z;

    asm (
    "BLUE: "
    "subb %%dl, %%al; "
    "ja GREEN; "
    "movb $0x00, %%al; "

    "GREEN: "
    "subb %%dl, %%ah; "
    "ja RED; "
    "movb $0x00, %%ah; "

    "RED: "
    "rol $0x10, %%eax; "
    "subb %%dl, %%al; "
    "ja DONE; "
    "movb $0x00, %%al; "

    "DONE: "
    "rol $0x10, %%eax; "
    : "=a" (z)
    : "a" (y),"d" (n)
    : "cc" );
    
    return z;
}

/*
unsigned long cyan_darkdun24
    (unsigned long x, unsigned long y, unsigned long n) 
{
    register int z;

    if (getr(y) == 0 && getg(y) > 210 && getb(y) == getg(y))
        return y;
    
    asm ("
    CBLUE:
    subb %%dl, %%al;
    ja CGREEN;
    movb $0x00, %%al;

    CGREEN:
    subb %%dl, %%ah;
    ja CRED;
    movb $0x00, %%ah;

    CRED:
    rol $0x10, %%eax;
    subb %%dl, %%al;
    ja CDONE;
    movb $0x00, %%al;

    CDONE:
    rol $0x10, %%eax;
    "
    : "=a" (z)
    : "a" (y),"d" (n)
    : "cc" );
    
    return z;    
}
*/

/*
// CHANGE THIS in _blend_alpha32
n = geta32(x) * n / 255;//scale the value of the alpha channel according to n
//normal alpha blending continues below

unsigned long factor = 128;//i.e. halfway blending
set_blender_mode_ex(NULL , NULL , NULL , blender_trans_alpha32 , NULL , NULL , NULL , 0,0,0,factor);
//and now call draw_trans_sprite

*/

unsigned long DSB_blender_fixalpha32(unsigned long x, unsigned long y, unsigned long n) {
   unsigned long res, g;
   unsigned long newalpha;

   n = geta32(x);
   
   newalpha = (geta32(y) + n);
   newalpha = ((newalpha > 255)? 255 : newalpha) << 24;

   if (n)
      n++;

   res = ((x & 0xFF00FF) - (y & 0xFF00FF)) * n / 256 + y;
   y &= 0xFF00;
   x &= 0xFF00;
   g = (x - y) * n / 256 + y;

   res &= 0xFF00FF;
   g &= 0xFF00;

   return (newalpha | (res | g));
}

void set_dsb_fixed_alpha_blender(int alphafix) {
    if (alphafix)
        set_blender_mode_ex(NULL, NULL, NULL, DSB_blender_fixalpha32, NULL, NULL, NULL, 0, 0, 0, 0);
    else
        set_alpha_blender();    
}
