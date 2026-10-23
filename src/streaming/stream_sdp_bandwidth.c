/* SPDX-License-Identifier: GPL-3.0-only */
/* Adapted from XCloud4, commit 008f4abe4f777fafa4259fe081ebfab065eed2d5. */
#include "stream_sdp_bandwidth.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Protocol research only: Better xCloud typescript f8397043f6d2148d2345d508902a38c69cf1ee20,
 * src/utils/sdp.ts:55-114 patchSdpBitrate uses media-level b=AS in kbps.
 * Independent bounded line walker; no upstream implementation is copied. */
static bool starts(const char *s,size_t n,const char *v)
{size_t m=strlen(v);return n>=m&&!memcmp(s,v,m);}
static bool video(const char *s,size_t n)
{return starts(s,n,"m=video")&&(n==7||s[7]==' '||s[7]=='\t');}
static size_t next_line(const char *s,size_t len,size_t pos,size_t *content)
{
    size_t end=pos;while(end<len&&s[end]!='\n')++end;
    *content=end-pos;if(*content&&s[end-1]=='\r')--*content;
    return end<len?end+1:end;
}
int x4_stream_sdp_limit_video(char *sdp,size_t capacity,unsigned kbps)
{
    if(!sdp||!capacity||!kbps)return -1;
    size_t length=0;while(length<capacity&&sdp[length])++length;
    if(length==capacity)return -1;
    if(capacity>SIZE_MAX/2)return -1;
    char *out=malloc(capacity);if(!out)return -2;
    size_t used=0,pos=0;bool in_video=false,wrote=false;unsigned sections=0;
    const char *newline=strstr(sdp,"\r\n")?"\r\n":"\n";
    char bandwidth[48];int bn=snprintf(bandwidth,sizeof(bandwidth),"b=AS:%u%s",kbps,newline);
    if(bn<1||(size_t)bn>=sizeof(bandwidth)){free(out);return -1;}
#define APPEND(src,n) do {size_t amount=(n);if(amount>=capacity-used){free(out);return -3;}memcpy(out+used,(src),amount);used+=amount;}while(0)
    while(pos<length){
        size_t content=0,end=next_line(sdp,length,pos,&content);
        bool media=starts(sdp+pos,content,"m=");
        if(media){
            if(in_video&&!wrote){APPEND(bandwidth,(size_t)bn);}
            in_video=video(sdp+pos,content);wrote=false;if(in_video)++sections;
        }else if(in_video&&starts(sdp+pos,content,"b=AS:")){
            if(!wrote){APPEND(bandwidth,(size_t)bn);wrote=true;}
            pos=end;continue; /* Normalize duplicate video AS lines. */
        }else if(in_video&&!wrote&&!starts(sdp+pos,content,"i=")&&!starts(sdp+pos,content,"c=")){
            APPEND(bandwidth,(size_t)bn);wrote=true;
        }
        APPEND(sdp+pos,end-pos);pos=end;
    }
    if(in_video&&!wrote){
        if(used&&out[used-1]!='\n')APPEND(newline,strlen(newline));
        APPEND(bandwidth,(size_t)bn);
    }
    out[used]=0;if(!sections){free(out);return -4;}
    memcpy(sdp,out,used+1);free(out);return 0;
#undef APPEND
}
