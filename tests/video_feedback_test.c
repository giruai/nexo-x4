#include "../src/streaming/video_feedback.h"
#include <assert.h>
#include <stdio.h>
#define CHECK(s,yes) assert(x4_video_remb_negotiated(s,sizeof(s)-1,102)==(yes))
int main(void) {
 CHECK("m=video 9 UDP/TLS/RTP/SAVPF 102\r\na=rtpmap:102 H264/90000\r\na=rtcp-fb:102 goog-remb\r\n",true);
 CHECK("m=video 9 UDP/TLS/RTP/SAVPF 102\na=rtcp-fb:* goog-remb\na=rtpmap:102 H264/90000\n",true);
 CHECK("m=video 0 UDP/TLS/RTP/SAVPF 102\na=rtpmap:102 H264/90000\na=rtcp-fb:102 goog-remb\n",false);
 CHECK("m=audio 9 UDP/TLS/RTP/SAVPF 102\na=rtpmap:102 H264/90000\na=rtcp-fb:102 goog-remb\n",false);
 CHECK("m=video 9 UDP/TLS/RTP/SAVPF 102\na=rtpmap:102 H264/90000\na=rtcp-fb:103 goog-remb\n",false);
 CHECK("m=video 9 UDP/TLS/RTP/SAVPF 102\na=rtpmap:102 H264/90000\na=rtcp-fb:102 goog-remb\na=inactive\n",false);
 CHECK("m=video 9 UDP/TLS/RTP/SAVPF 102\na=rtpmap:102 VP8/90000\na=rtcp-fb:102 goog-remb\n",false);
 CHECK("m=video 9 UDP/TLS/RTP/SAVPF 102\na=rtpmap:102 H264/90000\nm=audio 9 UDP/TLS/RTP/SAVPF 111\na=rtcp-fb:* goog-remb\n",false);
 puts("video_feedback: negotiated video/payload gating passed");return 0;
}
