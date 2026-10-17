/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/ui/psbox/overlay_copy.h"
#include <cassert>
#include <climits>
#include <vector>
#include <cstdio>
int main() {
    constexpr int w=31,h=23,sw=19,sh=17;
    std::vector<uint32_t> src(sw*sh);
    for (int i=0;i<sw*sh;++i) src[i]=0xff000000u+(unsigned)i;
    for (int x : {INT_MIN,-25,-6,0,3,20,40,INT_MAX}) for (int y : {INT_MIN,-20,-4,0,5,18,30,INT_MAX}) {
        std::vector<uint32_t> dst(w*h+2,0x12345678), expected=dst;
        for(int sy=0;sy<sh;++sy) for(int sx=0;sx<sw;++sx)
            if((int64_t)x+sx>=0&&(int64_t)x+sx<w&&(int64_t)y+sy>=0&&(int64_t)y+sy<h)
                expected[1+(y+sy)*w+x+sx]=src[sy*sw+sx];
        x4_overlay_copy(dst.data()+1,w,h,src.data(),sw,sh,x,y);
        assert(dst==expected);
    }
    puts("overlay_copy_test: clipped and unaligned paths ok");
}
