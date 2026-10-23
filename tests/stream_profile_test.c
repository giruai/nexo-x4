/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/streaming/stream_profile.h"
#include "../src/auth/device_info.h"
#include "../src/auth/json.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    char out[2048];
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
        if (profile == X4_STREAM_1080) {
            assert(!strcmp(out, "{}"));
            assert(p.web_windows && p.video_sdp_kbps == 8000 && p.max_kbps == 8000 && p.min_kbps == 5000);
            assert(p.max_fs == 8704 && p.max_mbps == 522240);
        } else {
            assert(!p.web_windows && !p.video_sdp_kbps && p.max_kbps == 8000 && p.min_kbps == 3000);
            assert(strstr(out, "\"supportsCustomResolution\":true"));
            assert(strstr(out, "\"maxWidth\":1280,\"maxHeight\":720,\"maxBitrateKbps\":8000"));
        }
        assert(x4_stream_profile_capabilities(p, out, 2) < 0 && out[0] == 0);
        assert(x4_stream_profile_dimensions(p,out,sizeof(out))>0);
        assert(x4_json_parse(out,strlen(out),&root)==0);
        assert(strstr(out, profile == X4_STREAM_1080 ? "\"horizontal\":1920,\"vertical\":1080" : "\"horizontal\":1280,\"vertical\":720"));
        const char *device = x4_xbox_device_info(profile);
        assert(x4_json_parse(device, strlen(device), &root) == 0);
        assert(strstr(device, profile == X4_STREAM_1080 ? "\"clientAppId\":\"www.xbox.com\"" : "\"clientAppId\":\"Nexo X4\""));
        assert(strstr(device, profile == X4_STREAM_1080 ? "\"widthInPixels\":4096,\"heightInPixels\":2160" : "\"widthInPixels\":1920,\"heightInPixels\":1080"));
        assert(x4_xbox_play_body(profile, "FORTNITE-123", out, sizeof(out)) > 0);
        assert(x4_json_parse(out, strlen(out), &root) == 0);
        assert(strstr(out, profile == X4_STREAM_1080 ? "\"osName\":\"windows\"" : "\"osName\":\"Orbis\""));
        assert(strstr(out, "\"locale\":\"es-MX\""));
        assert(strstr(out, "\"timezoneOffsetMinutes\":360"));
        assert(x4_xbox_play_body(profile, "FORTNITE-123", out, 8) < 0 && out[0] == 0);
    }
    assert(x4_xbox_play_body(0, "bad\"id", out, sizeof(out)) < 0 && out[0] == 0);
    assert(x4_xbox_play_body(0, NULL, out, sizeof(out)) < 0 && out[0] == 0);
    assert(x4_xbox_play_body(0, "", out, sizeof(out)) < 0 && out[0] == 0);
    assert(x4_xbox_play_body(9, "id", out, sizeof(out)) < 0 && out[0] == 0);
    X4StreamProfile captured = x4_stream_profile_get(X4_STREAM_1080);
    (void)x4_stream_profile_get(X4_STREAM_720);
    assert(captured.web_windows && captured.video_sdp_kbps == captured.max_kbps);

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
    puts("stream_profile_test: native 720 and G8 1080 serialized requests passed");
}
