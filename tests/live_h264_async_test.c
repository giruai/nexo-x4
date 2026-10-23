/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercises real converter/copy pthreads with the existing native ABI fixture. */
#define X4_TEST_THREADS 1
#define main synchronous_fixture_main
#include "live_h264_test.c"
#undef main
#include <sched.h>
static pthread_mutex_t sink_gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sink_condition = PTHREAD_COND_INITIALIZER;
static unsigned acquired, published, leased, block_sink, sink_entered;
static uint64_t sink_output;
static unsigned sink_width, sink_height;
static pthread_t decoder_owner;
static uint32_t expected_gray(uint64_t output)
{
    unsigned gray=(298u*(X4_TEST_FRAME_Y(output)-16u)+128u)>>8;
    return 0xff000000u|gray|(gray<<8)|(gray<<16);
}
static uint32_t *sink_acquire(void *context, size_t count)
{
    assert(context == pixels && count <= (size_t)1920*1080);
    assert(!pthread_equal(pthread_self(),decoder_owner));
    pthread_mutex_lock(&sink_gate);
    assert(!leased); leased=1; ++acquired; sink_entered=1;
    pthread_cond_broadcast(&sink_condition);
    while (block_sink) pthread_cond_wait(&sink_condition,&sink_gate);
    pthread_mutex_unlock(&sink_gate);
    return pixels;
}
static void sink_publish(void *context,unsigned w,unsigned h,uint64_t output,uint64_t completed)
{
    assert(context == pixels && completed);
    assert(!pthread_equal(pthread_self(),decoder_owner));
    /* Output 1 stays blocked while another 20 pictures reuse the native ring.
     * Both ends must retain this output's own bytes, including bottom crop. */
    assert(pixels[0]==expected_gray(output));
    assert(pixels[(size_t)w*h-1]==expected_gray(output));
    pthread_mutex_lock(&sink_gate);
    assert(leased); leased=0; ++published;
    sink_width=w;sink_height=h;sink_output=output;
    pthread_cond_broadcast(&sink_condition);
    pthread_mutex_unlock(&sink_gate);
}
static void wait_entered(void)
{
    pthread_mutex_lock(&sink_gate);
    while (!sink_entered) pthread_cond_wait(&sink_condition,&sink_gate);
    pthread_mutex_unlock(&sink_gate);
}
static void release_sink(void)
{
    pthread_mutex_lock(&sink_gate);block_sink=0;
    pthread_cond_broadcast(&sink_condition);pthread_mutex_unlock(&sink_gate);
}
static void wait_published(unsigned count)
{
    pthread_mutex_lock(&sink_gate);
    while(published<count) pthread_cond_wait(&sink_condition,&sink_gate);
    pthread_mutex_unlock(&sink_gate);
}
static void reset_sink(void)
{
    pthread_mutex_lock(&sink_gate);
    assert(!leased);acquired=published=sink_entered=block_sink=0;
    pthread_mutex_unlock(&sink_gate);
}
static X4LiveVideo begin_async(int profile)
{
    reset_sink();X4LiveVideo v=begin(profile);
    v.sink=(X4LiveVideoSink){pixels,sink_acquire,sink_publish};
    assert(!x4_live_video_start(&v,pixels,(size_t)1920*1080));return v;
}
static void gpu_failure_cases(void)
{
    uint8_t au[512];size_t n=sps_au(au,1920,1088,8,77,1);
    for(unsigned mode=0;mode<3;++mode) {
        X4LiveVideo v=begin_async(X4_STREAM_1080);
        gpu_enabled=mode!=0;gpu_safe_reject=mode==1;gpu_mismatch=mode==2;
        assert(!x4_live_video_feed(&v,au,n,1));wait_published(1);
        assert(!v.direct_jobs&&v.direct_fallback_jobs==1&&!v.wb_copy_calls);
        VideoState *s=v.state;
        assert(s->conversion_jobs[0].allocation&&!s->conversion_jobs[0].readback);
        assert(!x4_live_video_stop(&v));assert(!gpu_contexts&&!live_reservations);
    }
    { /* A negative transfer may still own native source + destination/commands.
       * No decode/reuse or Delete can follow without completion evidence. */
        X4LiveVideo v=begin_async(X4_STREAM_1080);gpu_enabled=1;gpu_transfer_error=-88;
        assert(x4_live_video_feed(&v,au,n,1)==-88);
        VideoState *s=v.state;unsigned owned=live_reservations,before=decode_calls;
        assert(s->pending_valid&&s->conversion_queue.state[0]==X4_CONVERSION_COPYING);
        assert(x4_live_video_feed(&v,au,n,2)<0&&decode_calls==before);
        assert(x4_live_video_stop(&v)==-88&&v.state==s&&active_decoder&&!delete_calls);
        assert(live_reservations==owned&&gpu_contexts==3&&s->conversion_jobs[0].readback->inflight);
        /* Fixture-only late completion proof; production has no force-free. */
        s->conversion_jobs[0].readback->inflight=false;gpu_transfer_error=0;
        assert(!x4_live_video_stop(&v));assert(!gpu_contexts&&!live_reservations);
    }
    for(unsigned native_failure=0;native_failure<2;++native_failure) {
        X4LiveVideo v=begin_async(X4_STREAM_1080);gpu_enabled=1;
        assert(!x4_live_video_feed(&v,au,n,1));wait_published(1);
        VideoState *s=v.state;unsigned owned=live_reservations;
        if(native_failure)delete_error=-89;else gpu_close_error=-87;
        assert(x4_live_video_stop(&v)==(native_failure?-89:-87));
        assert(v.state==s&&live_reservations==owned&&active_decoder&&v.pixels==pixels);
        if(!native_failure)assert(!delete_calls&&gpu_contexts==3);
        gpu_close_error=delete_error=0;
        assert(!x4_live_video_stop(&v));assert(!gpu_contexts&&!live_reservations);
    }
    { /* Safe rejection cannot fall back if retiring its context fails. */
        X4LiveVideo v=begin_async(X4_STREAM_1080);gpu_enabled=gpu_safe_reject=1;gpu_close_error=7;
        assert(x4_live_video_feed(&v,au,n,1)==-7);
        VideoState *s=v.state;assert(s->pending_valid&&!v.direct_fallback_jobs&&!v.conversion_submitted);
        assert(x4_live_video_stop(&v)==-7&&!delete_calls&&gpu_contexts==3);
        gpu_close_error=0;assert(!x4_live_video_stop(&v));assert(!gpu_contexts&&!live_reservations);
    }
    { /* A partial open with a retained context never gets overwritten or
       * bypassed by Delete; an earlier independently safe context may close. */
        X4LiveVideo v=begin_async(X4_STREAM_1080);gpu_enabled=1;gpu_open_error_at=2;
        assert(x4_live_video_feed(&v,au,n,1)==-86);
        VideoState *s=v.state;unsigned owned=live_reservations;
        assert(!s->conversion_running&&gpu_contexts==2&&s->conversion_jobs[1].readback);
        assert(x4_live_video_stop(&v)==-86&&!delete_calls&&active_decoder);
        assert(!s->conversion_jobs[0].readback&&gpu_contexts==1&&live_reservations==owned);
        s->conversion_jobs[1].readback->close_error=0;
        assert(!x4_live_video_stop(&v));assert(!gpu_contexts&&!live_reservations);
    }
    { /* The720 selected route keeps its existing singleton+WB copy behavior. */
        X4LiveVideo v=begin_async(X4_STREAM_720);gpu_enabled=1;
        n=sps_au(au,1280,720,0,66,1);au[7]=32;
        assert(!x4_live_video_feed(&v,au,n,1));wait_published(1);
        VideoState *s=v.state;
        assert(!s->direct_readback&&gpu_contexts==1&&v.wb_copy_calls==1&&!v.direct_jobs);
        assert(!x4_live_video_stop(&v));assert(!gpu_contexts&&!live_reservations);
    }
}
int main(void)
{
    decoder_owner=pthread_self();
    fixture_native_owner=decoder_owner;
    pixels=malloc((size_t)1920*1080*sizeof(*pixels));assert(pixels);
    uint8_t au[512];size_t n=sps_au(au,1920,1088,8,77,1);
    X4LiveVideo v=begin(X4_STREAM_1080);
    gpu_enabled=1;
    v.sink=(X4LiveVideoSink){pixels,sink_acquire,sink_publish};
    /* start preserves the sink just as a decoder recovery restart does. */
    assert(!x4_live_video_start(&v,pixels,(size_t)1920*1080));
    output_pitch=1984;
    block_sink=1;
    assert(!x4_live_video_feed(&v,au,n,1)); wait_entered();
    VideoState *initial=v.state;int initial_slot=-1;
    conversion_lock(initial);
    for(unsigned i=0;i<X4_CONVERSION_SLOTS;++i)
        if(initial->conversion_queue.state[i]==X4_CONVERSION_ACTIVE)initial_slot=(int)i;
    conversion_unlock(initial);assert(initial_slot>=0);
    size_t active_bytes=(size_t)output_pitch*1088*3/2;
    uint8_t *active_snapshot=malloc(active_bytes);assert(active_snapshot);
    memcpy(active_snapshot,initial->conversion_jobs[initial_slot].nv12,active_bytes);
    /* Converter holds a WB slot/RGB lease while owner keeps decoding/copying.
     * Saturate latest replacement; no write can touch that active WB slot. */
    for(unsigned i=0;i<20;++i) assert(!x4_live_video_feed(&v,au,n,i+2));
    VideoState *s=v.state;
    assert(!memcmp(active_snapshot,s->conversion_jobs[initial_slot].nv12,active_bytes));
    free(active_snapshot);
    assert(v.decoded_frames==21 && published==0);
    assert(gpu_contexts==3 && !s->gpu_readback && gpu_open_calls==3);
    assert(v.direct_jobs==21 && v.gpu_transfer_calls==21 && !v.wb_copy_calls);
    assert(v.direct_bytes==(uint64_t)1984*1088*3/2*21);
    assert(v.gpu_check_calls==2&&!v.gpu_check_mismatch&&v.gpu_check_us);
    assert(v.gpu_check_bytes==(uint64_t)1984*1088*3/2*2);
    for(unsigned i=0;i<X4_CONVERSION_SLOTS;++i) {
        assert(!s->conversion_jobs[i].allocation);
        if(s->conversion_queue.state[i]==X4_CONVERSION_ACTIVE)
            assert(s->conversion_jobs[i].nv12==s->conversion_jobs[i].readback->data);
    }
    unsigned active=0,ready=0;
    conversion_lock(s);
    for(unsigned i=0;i<X4_CONVERSION_SLOTS;++i){
        active+=s->conversion_queue.state[i]==X4_CONVERSION_ACTIVE;
        ready+=s->conversion_queue.state[i]==X4_CONVERSION_READY;
    }
    conversion_unlock(s);assert(active==1&&ready==1);
    release_sink();wait_published(2);
    assert(!x4_live_video_stop(&v));
    assert(!v.state&&!live_reservations&&!gpu_contexts&&published==2&&acquired==2);
    assert(v.frames==2&&v.convert_us&&sink_width==1920&&sink_height==1080&&sink_output==21);
    assert(v.conversion_submitted==21&&v.conversion_dropped==19);
    for(size_t i=0;i<(size_t)1920*1080;++i) assert(pixels[i]==expected_gray(21));
    /* Restart carries session trace identities, fresh immutable job geometry. */
    assert(!x4_live_video_start(&v,pixels,(size_t)1920*1080));
    output_pitch=0;
    n=sps_au(au,1280,720,0,66,1);au[7]=32;
    assert(!x4_live_video_feed(&v,au,n,1));wait_published(3);
    assert(!x4_live_video_stop(&v));assert(sink_width==1280&&sink_height==720&&sink_output==22);
    /* Failed join must retain decoder/native/WB/RGB ownership wholesale. */
    assert(!x4_live_video_start(&v,pixels,(size_t)1920*1080));
    assert(!x4_live_video_feed(&v,au,n,1));wait_published(4);
    s=v.state;fail_join=1;
    assert(x4_live_video_stop(&v)==-98 && v.state==s && active_decoder==1 && live_reservations);
    assert(gpu_contexts==3 && s->staging_allocation);
    fail_join=0;
    assert(x4_live_video_stop(&v)==-98); /* latched unknown lifetime */
    /* Fixture-only: prove actual thread completion before releasing retained
     * resources for the sanitizer; production never clears the failure latch. */
    assert(!pthread_join(s->conversion_thread,NULL));
    s->conversion_running=false;s->conversion_join_error=0;
    assert(!x4_live_video_stop(&v));assert(!live_reservations);
    /* Worker creation failure is explicit and partial WB allocations unwind. */
    assert(!x4_live_video_start(&v,pixels,(size_t)1920*1080));fail_create=1;
    assert(x4_live_video_feed(&v,au,n,1)==-99);
    fail_create=0;assert(!x4_live_video_stop(&v));assert(!live_reservations);
    gpu_failure_cases();
    free(pixels);puts("async NV12 ownership, latest job, restart and failure retention passed");
}
