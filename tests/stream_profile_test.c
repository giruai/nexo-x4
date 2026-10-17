/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/streaming/stream_profile.h"
#include "../src/auth/device_info.h"
#include "../src/auth/json.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    char out[512];
    for (int profile=0; profile<2; ++profile) {
        X4StreamProfile p=x4_stream_profile_get(profile);
        assert(p.fps==60 && p.min_kbps<=p.max_kbps);
        assert(x4_stream_profile_resolution_message(p,out,sizeof(out))>0);
        X4JsonSpan root,v;
        char value[64];
        assert(x4_json_parse(out,strlen(out),&root)==0);
        assert(x4_json_member(root,"message",&v)==1 && x4_json_token(v,value,sizeof(value))>=0);
        assert(!strcmp(value,"userRequestedResolutionUpdate"));
        assert(x4_json_member(root,"resolutionAlias",&v)==1 && x4_json_token(v,value,sizeof(value))>=0);
        assert(!strcmp(value,profile==X4_STREAM_1080?"1080":"720"));
        assert(x4_stream_profile_resolution_message(p,out,8)<0 && out[0]==0);
        assert(x4_stream_profile_capabilities(p,out,sizeof(out))>0);
        assert(x4_json_parse(out,strlen(out),&root)==0);
        assert(x4_stream_profile_dimensions(p,out,sizeof(out))>0);
        assert(x4_json_parse(out,strlen(out),&root)==0);
    }
    X4StreamProfile invalid=x4_stream_profile_get(0);invalid.width=3840;
    assert(x4_stream_profile_resolution_message(invalid,out,sizeof(out))<0 && out[0]==0);
    assert(strstr(X4_XBOX_DEVICE_INFO,"\"clientAppVersion\":\"" X4_PRODUCT_VERSION "\""));
    assert(strstr(X4_XBOX_DEVICE_INFO,"\"name\":\"Orbis\""));
    X4JsonSpan root,dev,display,dims,v;uint32_t width=0,height=0;
    assert(x4_json_parse(X4_XBOX_DEVICE_INFO,strlen(X4_XBOX_DEVICE_INFO),&root)==0);
    assert(x4_json_member(root,"dev",&dev)==1 && x4_json_member(dev,"displayInfo",&display)==1);
    assert(x4_json_member(display,"dimensions",&dims)==1);
    assert(x4_json_member(dims,"widthInPixels",&v)==1 && x4_json_uint32(v,&width)==0);
    assert(x4_json_member(dims,"heightInPixels",&v)==1 && x4_json_uint32(v,&height)==0);
    assert(width==1920 && height==1080);
    puts("stream_profile_test: base resolution request and public display capabilities passed");
}
