#include <allegro.h>
#include <winalleg.h>
#include <math.h>
#include "callstack.h"
#include "distort.h"

const int VP_DX = 448;
const int VP_DY = 272;

void distort_viewport(int process, BITMAP *dest, int bx, int by, int t_val) {
    static BITMAP *melt_fade_bitmap = NULL; 
    int xc, yc;
    BITMAP *temporary;
    onstack("distort_viewport");
    
    temporary = create_bitmap(VP_DX, VP_DY);
    
    if (process != DISTORT_MELTFADE) {
        if (melt_fade_bitmap) {
            destroy_bitmap(melt_fade_bitmap);
            melt_fade_bitmap = NULL;
        }
    }
    
    if (process == DISTORT_UNDERWATER) {
        for(yc=0;yc<VP_DY;++yc) {
            for(xc=0;xc<VP_DX;++xc) {
                int pxl;
                int xo, yo;
                int sx, sy;
                xo = (4 * sin(2.0 * M_PI * ((double)yc / 128.0 + (double)t_val / 256.0)));
                yo = (4 * cos(2.0 * M_PI * ((double)xc / 128.0 + (double)t_val / 256.0)));  
                
                sx = ((xc + xo + VP_DX) % VP_DX);
                sy = ((yc + yo + VP_DY) % VP_DY);        
                
                pxl = _getpixel32(dest, bx + sx, by + sy);
                _putpixel32(temporary, xc, yc, pxl);         
            }
        }
    } else if (process == DISTORT_BLUR) {
        blit(dest, temporary, bx, by, 0, 0, VP_DX, 1);
        blit(dest, temporary, bx, by + (VP_DY-1), 0, VP_DY-1, VP_DX, 1);
        
        blit(dest, temporary, bx, by, 0, 0, 1, VP_DY);
        blit(dest, temporary, bx + (VP_DX-1), by, VP_DX-1, 0, 1, VP_DY);
            
        for(yc=1;yc<(VP_DY - 1);++yc) {
            for(xc=1;xc<(VP_DX - 1);++xc) {
                int dx, dy;
                int pxl;            
                int rv, gv, bv;  
                pxl = _getpixel32(dest, bx + xc, by + yc);
                rv = getr(pxl);
                gv = getg(pxl);
                bv = getb(pxl);
                for(dy=-1;dy<=1;++dy) {
                    for(dx=-1;dx<=1;++dx) {
                        int tpxl = _getpixel32(dest, bx + xc + dx, by + yc + dy);
                        rv += getr(tpxl);
                        gv += getg(tpxl);
                        bv += getb(tpxl);
                    }
                }
                rv = rv / 10;
                gv = gv / 10;
                bv = bv / 10;
                pxl = makecol(rv, gv, bv);
                _putpixel32(temporary, xc, yc, pxl);         
            }
        }
        
    } else if (process == DISTORT_STATIC) {
        for(yc=0;yc<VP_DY;++yc) {
            int randv_a = DSBtrivialrand() % 32;
            int randv_d = DSBtrivialrand() % 32;
            int bright = 64 + (DSBtrivialrand() % 56); 
            for(xc=0;xc<VP_DX;++xc) {
                int rv, gv, bv;
                int pxl = _getpixel32(dest, bx + xc, by + yc);
                rv = getr(pxl);
                gv = getg(pxl);
                bv = getb(pxl);
                
                if ((((xc * 17) + randv_a) % 57) == 0) {
                    int bc = bright - (xc % 31);
                    rv += bc, gv += bc, bv += bc;
                }
                
                gv = (gv + randv_a - randv_d);
                if (gv < 0) gv = 0;
                else if (gv > 255) gv = 255;
            
                rv = (rv + randv_a - randv_d);
                if (rv < 0) rv = 0;
                else if (rv > 255) rv = 255;
                
                bv = (bv + randv_a - randv_d);
                if (bv < 0) bv = 0;
                else if (bv > 255) bv = 255;
                                
                pxl = makecol(rv, gv, bv);
                _putpixel32(temporary, xc, yc, pxl);         
            }
        }        
        
    } else if (process == DISTORT_MELTFADE) {
        int z;
        int it_s[8] = { 64, 49, 36, 25, 12, 6, 4, 0 };
        int it_n[8] = { 8, 7, 6, 5, 4, 3, 2, 1 };
        
        if (!melt_fade_bitmap) {
            melt_fade_bitmap = create_bitmap(VP_DX, VP_DY); 
            blit(dest, melt_fade_bitmap, bx, by, 0, 0, VP_DX, VP_DY);
        }
        
        for(yc=0;yc<VP_DY;++yc) {
            for(xc=0;xc<VP_DX;++xc) {
                int pxl = _getpixel32(dest, bx + xc, by + yc);
                int mf_pxl = _getpixel32(melt_fade_bitmap, xc, yc);
                
                if (pxl != mf_pxl) {
                    int pv[3], mf_pv[3];
                    int i;
                    pv[0] = getr(pxl), mf_pv[0] = getr(mf_pxl);
                    pv[1] = getg(pxl), mf_pv[1] = getg(mf_pxl);
                    pv[2] = getb(pxl), mf_pv[2] = getb(mf_pxl);
                    
                    for (i=0;i<3;++i) {
                        int dv = pv[i] - mf_pv[i];
                        for (z=0;z<8;++z) {
                            if (dv > it_s[z]) {
                                mf_pv[i] += it_n[z];
                                break;
                            } else if (dv < (-1 * it_s[z])) {
                                mf_pv[i] -= it_n[z];
                                break;
                            }
                        }
                    }

                    pxl = makecol(mf_pv[0], mf_pv[1], mf_pv[2]);
                    _putpixel32(melt_fade_bitmap, xc, yc, pxl); 
                }
            
                _putpixel32(temporary, xc, yc, pxl);         
            }
        }  
               
    }   
    
    blit(temporary, dest, 0, 0, bx, by, VP_DX, VP_DY);
    destroy_bitmap(temporary);
    
    VOIDRETURN();
}
