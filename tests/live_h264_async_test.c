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
int main(void)
{
    decoder_owner=pthread_self();
    fixture_native_owner=decoder_owner;
    pixels=malloc((size_t)1920*1080*sizeof(*pixels));assert(pixels);
    uint8_t au[512];size_t n=sps_au(au,1920,1088,8,77,1);
    X4LiveVideo v=begin(X4_STREAM_1080);
    v.sink=(X4LiveVideoSink){pixels,sink_acquire,sink_publish};
    /* start preserves the sink just as a decoder recovery restart does. */
    assert(!x4_live_video_start(&v,pixels,(size_t)1920*1080));
    output_pitch=1984;
    block_sink=1;
    assert(!x4_live_video_feed(&v,au,n,1)); wait_entered();
    /* Converter holds a WB slot/RGB lease while owner keeps decoding/copying.
     * Saturate latest replacement; no write can touch that active WB slot. */
    for(unsigned i=0;i<20;++i) assert(!x4_live_video_feed(&v,au,n,i+2));
    VideoState *s=v.state;
    assert(v.decoded_frames==21 && published==0);
    unsigned active=0,ready=0;
    conversion_lock(s);
    for(unsigned i=0;i<X4_CONVERSION_SLOTS;++i){
        active+=s->conversion_queue.state[i]==X4_CONVERSION_ACTIVE;
        ready+=s->conversion_queue.state[i]==X4_CONVERSION_READY;
    }
    conversion_unlock(s);assert(active==1&&ready==1);
    release_sink();wait_published(2);
    assert(!x4_live_video_stop(&v));
    assert(!v.state&&!live_reservations&&published==2&&acquired==2);
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
    assert(s->conversion_jobs[0].allocation && s->staging_allocation);
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
    free(pixels);puts("async NV12 ownership, latest job, restart and failure retention passed");
}
