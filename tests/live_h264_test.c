/* SPDX-License-Identifier: GPL-3.0-only */
/* Native API fixture: verifies caller policy and lifetimes, not driver behavior. */
#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/video/live_h264.c"

#ifdef X4_TEST_THREADS
static atomic_uint_fast64_t clock_us = 1;
static int fail_join, fail_create;
static pthread_t fixture_native_owner;
#define ASSERT_NATIVE_OWNER() assert(pthread_equal(pthread_self(),fixture_native_owner))
#define X4_TEST_FRAME_Y(output) (16u + ((unsigned)(output) * 7u) % 219u)
#else
static uint64_t clock_us = 1;
#define ASSERT_NATIVE_OWNER() ((void)0)
#endif
static unsigned query_calls, create_calls, decode_calls, delete_calls, active_decoder;
static unsigned output_height, output_pitch, no_picture, bad_pointer, bad_size, oversized_memory;
static int delete_error, gpu_close_error, create_null;
static OrbisVideodec2DecoderConfigInfo queried;
static void *previous_input;
static uint8_t previous_first;
typedef struct { void *p; size_t size; } Reservation;
static Reservation reservations[64];
static unsigned live_reservations;
uint64_t sceKernelGetProcessTime(void) { return clock_us++; }
int sceKernelUsleep(unsigned us) { clock_us += us; return 0; }
int scePthreadCreate(OrbisPthread *a,const pthread_attr_t *b,void *(*c)(void *),void *d,const char *e)
{
#ifdef X4_TEST_THREADS
    (void)e; if (fail_create) return -99; return pthread_create(a,b,c,d);
#else
    (void)a;(void)b;(void)c;(void)d;(void)e;return -1;
#endif
}
int scePthreadJoin(OrbisPthread t,void **r) {
#ifdef X4_TEST_THREADS
    if (fail_join) return -98;
    return pthread_join(t,r);
#else
    (void)t;(void)r;assert(0);return -1;
#endif
}
size_t sceKernelGetDirectMemorySize(void) { return 1024u*1024u*1024u; }
int sceKernelAllocateDirectMemory(off_t a,off_t b,size_t size,size_t align,int type,off_t *offset)
{
    (void)a;(void)b;(void)type;
    for(unsigned i=1;i<64;++i) if(!reservations[i].p) {
        assert(!posix_memalign(&reservations[i].p,align,size));
        reservations[i].size=size;*offset=i;++live_reservations;return 0;
    }
    return -1;
}
int sceKernelMapDirectMemory(void **p,size_t size,int prot,int flags,off_t offset,size_t align)
{ (void)prot;(void)flags;(void)align;assert(reservations[offset].size==size);*p=reservations[offset].p;return 0; }
int sceKernelMunmap(void *p,size_t size)
{ (void)p;(void)size;assert(!active_decoder);return 0; }
int sceKernelReleaseDirectMemory(off_t offset,size_t size)
{
    assert(!active_decoder);assert(reservations[offset].size==size);
    free(reservations[offset].p);reservations[offset]=(Reservation){0};--live_reservations;return 0;
}
/* Bounded readback ownership fixture; real DMA/cache correctness is hardware
 * coverage, while these buffers make lease violations visible to sanitizers. */
struct X4GpuReadback { uint8_t *data; size_t capacity; bool inflight; int close_error; };
static unsigned gpu_enabled, gpu_contexts, gpu_open_calls, gpu_transfer_calls;
static unsigned gpu_mismatch, gpu_safe_reject;
static int gpu_transfer_error;
static unsigned gpu_open_error_at;
int x4_gpu_readback_open(X4GpuReadback **g,size_t n)
{
    ASSERT_NATIVE_OWNER();++gpu_open_calls;
    if(!gpu_enabled){*g=NULL;return 1;}
    assert(!*g);*g=calloc(1,sizeof(**g));assert(*g);
    (*g)->data=malloc(n);assert((*g)->data);(*g)->capacity=n;++gpu_contexts;
    if(gpu_open_error_at==gpu_open_calls){(*g)->close_error=-86;return -86;}
    return 0;
}
int x4_gpu_readback_transfer(X4GpuReadback *g,const void *p,size_t n)
{
    ASSERT_NATIVE_OWNER();assert(g&&n<=g->capacity&&!g->inflight);++gpu_transfer_calls;
    if(gpu_transfer_error){g->inflight=true;return gpu_transfer_error;}
    if(gpu_safe_reject)return 1;
    memcpy(g->data,p,n);if(gpu_mismatch)g->data[n-1]^=1;return 0;
}
const uint8_t *x4_gpu_readback_data(const X4GpuReadback *g)
{ ASSERT_NATIVE_OWNER();return g&&!g->inflight?g->data:NULL; }
bool x4_gpu_readback_inflight(const X4GpuReadback *g) { return g&&g->inflight; }
int x4_gpu_readback_close(X4GpuReadback **g)
{
    ASSERT_NATIVE_OWNER();if(gpu_close_error)return gpu_close_error;
    if(!*g)return 0;if((*g)->inflight)return -88;
    if((*g)->close_error)return (*g)->close_error;
    free((*g)->data);free(*g);*g=NULL;--gpu_contexts;return 0;
}
void x4_trace_record(X4Trace *t,uint16_t e,uint16_t f,uint64_t a,uint64_t b)
{ (void)t;(void)e;(void)f;(void)a;(void)b; }
static int32_t query_compute(OrbisVideodec2ComputeMemoryInfo *m) { ASSERT_NATIVE_OWNER();m->cpuGpuMemorySize=16384;return 0; }
static int32_t allocate_queue(const OrbisVideodec2ComputeConfigInfo *c,const OrbisVideodec2ComputeMemoryInfo *m,void **q)
{ ASSERT_NATIVE_OWNER();(void)c;assert(m->pCpuGpuMemory);*q=(void *)1;return 0; }
static int32_t release_queue(void *q) { ASSERT_NATIVE_OWNER();assert(q);assert(!active_decoder);return 0; }
static int32_t query_decoder(const OrbisVideodec2DecoderConfigInfo *c,OrbisVideodec2DecoderMemoryInfo *m)
{
    ASSERT_NATIVE_OWNER();++query_calls;queried=*c;assert(c->decodePipelineDepth==1);
    m->cpuMemorySize=m->gpuMemorySize=m->cpuGpuMemorySize=oversized_memory?64u*1024u*1024u:16384;
    m->maxFrameBufferSize=(size_t)(c->maxFrameWidth+64)*c->maxFrameHeight*3/2;
    m->frameBufferAlignment=16384;return 0;
}
static int32_t create_decoder(const OrbisVideodec2DecoderConfigInfo *c,const OrbisVideodec2DecoderMemoryInfo *m,void **d)
{ ASSERT_NATIVE_OWNER();assert(!memcmp(c,&queried,sizeof(*c)));assert(m->pGpuMemory);++create_calls;++active_decoder;*d=create_null?NULL:(void *)2;return 0; }
static int32_t delete_decoder(void *d)
{ ASSERT_NATIVE_OWNER();assert(d);assert(!gpu_contexts);++delete_calls;if(delete_error)return delete_error;--active_decoder;previous_input=NULL;return 0; }
static int32_t decode(void *d,const OrbisVideodec2InputData *i,OrbisVideodec2FrameBuffer *f,OrbisVideodec2OutputInfo *o)
{
    ASSERT_NATIVE_OWNER();assert(d&&active_decoder);assert(i->pAuData&&i->auSize);
    if(previous_input){assert(previous_input!=i->pAuData);assert(*(uint8_t *)previous_input==previous_first);}
    previous_input=i->pAuData;previous_first=*(uint8_t *)i->pAuData;
    ++decode_calls;f->isAccepted=true;
    if(no_picture)return 0;
    unsigned pitch=output_pitch?output_pitch:(unsigned)queried.maxFrameWidth;
    unsigned height=output_height?output_height:(unsigned)queried.maxFrameHeight;
    size_t bytes=(size_t)pitch*height*3/2;
    assert(bytes<=f->frameBufferSize);
    /* Padding luma is deliberately non-neutral: using visible height as the
     * UV offset for coded1088 would turn the first rows green/purple. */
#ifdef X4_TEST_THREADS
    /* Distinct native outputs expose active-WB overwrite and stale content. */
    memset(f->pFrameBuffer,X4_TEST_FRAME_Y(decode_calls),(size_t)pitch*height);
#else
    memset(f->pFrameBuffer,235,(size_t)pitch*height);
#endif
    if(height==1088)memset((uint8_t *)f->pFrameBuffer+(size_t)pitch*1080,0,(size_t)pitch*8);
    memset((uint8_t *)f->pFrameBuffer+(size_t)pitch*height,128,(size_t)pitch*height/2);
    o->isValid=true;o->codecType=1;o->frameWidth=queried.maxFrameWidth;o->frameHeight=height;
    o->framePitch=pitch;o->pFrameBuffer=bad_pointer?(void *)16:f->pFrameBuffer;
    o->frameBufferSize=bad_size?bytes-1:f->frameBufferSize;
    return 0;
}
static int32_t flush(void *d,OrbisVideodec2FrameBuffer *f,OrbisVideodec2OutputInfo *o)
{ ASSERT_NATIVE_OWNER();(void)d;(void)f;(void)o;return 0; }
int x4_module_open(const char *s) { assert(!strcmp(s,"libSceVideodec2"));return 1; }
int x4_module_symbol(int handle,const char *name,void **address)
{
    assert(handle==1);
#define BIND(n,fn) if(!strcmp(name,"sceVideodec2" #n)){*address=(void *)(fn);return 0;}
    BIND(QueryComputeMemoryInfo,query_compute) BIND(AllocateComputeQueue,allocate_queue)
    BIND(ReleaseComputeQueue,release_queue) BIND(QueryDecoderMemoryInfo,query_decoder)
    BIND(CreateDecoder,create_decoder) BIND(DeleteDecoder,delete_decoder) BIND(Decode,decode) BIND(Flush,flush)
#undef BIND
    return -1;
}
/* Deterministic SPS writer including byte-stream emulation prevention. */
static unsigned fixture_explicit_dpb;
typedef struct {uint8_t p[256];unsigned bit;} Writer;
static void put(Writer *w,unsigned n,unsigned x)
{for(unsigned i=0;i<n;++i){unsigned b=(x>>(n-i-1))&1;w->p[w->bit/8]|=b<<(7-w->bit%8);++w->bit;}}
static void golomb(Writer *w,unsigned x)
{unsigned code=x+1,n=0;for(unsigned v=code;v>1;v>>=1)++n;put(w,n,0);put(w,n+1,code);}
static size_t sps_au(uint8_t *p,unsigned width,unsigned coded_height,unsigned crop_bottom,unsigned profile,unsigned refs)
{
    Writer w={0};put(&w,8,profile);put(&w,8,0xc0);put(&w,8,42);golomb(&w,0);
    if(profile==100){golomb(&w,1);golomb(&w,0);golomb(&w,0);put(&w,1,0);put(&w,1,0);}
    golomb(&w,0);golomb(&w,0);golomb(&w,0);golomb(&w,refs);put(&w,1,0);
    golomb(&w,width/16-1);golomb(&w,coded_height/16-1);put(&w,1,1);put(&w,1,1);
    put(&w,1,crop_bottom!=0);if(crop_bottom){golomb(&w,0);golomb(&w,0);golomb(&w,0);golomb(&w,crop_bottom/2);}
    put(&w,1,fixture_explicit_dpb!=0);
    if(fixture_explicit_dpb){
        put(&w,8,0);put(&w,1,1);put(&w,1,1);
        for(unsigned i=0;i<4;++i)golomb(&w,0);
        golomb(&w,0);golomb(&w,fixture_explicit_dpb);
    }
    put(&w,1,1);while(w.bit%8)put(&w,1,0);
    memcpy(p,"\0\0\0\1\x67",5);size_t at=5;unsigned zeros=0;
    for(unsigned i=0;i<w.bit/8;++i){unsigned b=w.p[i];if(zeros>=2&&b<=3){p[at++]=3;zeros=0;}p[at++]=b;zeros=b?0:zeros+1;}
    memcpy(p+at,"\0\0\1\x65\x88",5);return at+5;
}
static uint32_t *pixels;
static X4LiveVideo begin(int profile)
{
    assert(!live_reservations&&!active_decoder&&!gpu_contexts);previous_input=NULL;
    gpu_enabled=gpu_open_calls=gpu_transfer_calls=gpu_mismatch=gpu_safe_reject=0;gpu_transfer_error=0;gpu_open_error_at=0;
    query_calls=create_calls=decode_calls=delete_calls=0;
    output_height=output_pitch=no_picture=bad_pointer=bad_size=oversized_memory=0;
    delete_error=gpu_close_error=create_null=0;fixture_explicit_dpb=0;
    X4LiveVideo v={.stream_profile=profile};
    assert(!x4_live_video_start(&v,pixels,(size_t)1920*1080));assert(!query_calls&&!create_calls);
    return v;
}
static void end(X4LiveVideo *v) { assert(!x4_live_video_stop(v));assert(!live_reservations&&!v->state); }
int main(void)
{
    const size_t capacity=(size_t)1920*1080;
    pixels=malloc((capacity+1)*sizeof(*pixels));assert(pixels);uint8_t au[512];size_t n;
    for(unsigned profile=66;profile<=100;profile+=34) {
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,8,profile,1);
        pixels[capacity]=0xdeadbeef;output_pitch=1984;
        assert(!x4_live_video_feed(&v,au,n,1));assert(queried.maxFrameHeight==1088&&queried.maxDpbFrameCount==4);
        assert(!x4_live_video_convert_pending(&v));assert(v.width==1920&&v.height==1080&&v.frames==1);
        for(size_t i=0;i<capacity;++i)assert(pixels[i]==0xffffffff);
        assert(pixels[capacity]==0xdeadbeef);end(&v);
    }
    { /* Native may report an already cropped/packed 1080-row surface. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,8,66,1);output_height=1080;
        assert(!x4_live_video_feed(&v,au,n,1));assert(!x4_live_video_convert_pending(&v));assert(pixels[0]==0xffffffff);end(&v);
    }
    { /* 720 regression, persistent inputs, and pending ring wrap preserve. */
        X4LiveVideo v=begin(X4_STREAM_720);n=sps_au(au,1280,720,0,66,1);
        /* Keep a level32 baseline in addition to the observed level42 transition. */
        au[7]=32;
        assert(!x4_live_video_feed(&v,au,n,1));no_picture=1;
        for(unsigned i=0;i<4;++i)assert(!x4_live_video_feed(&v,au,n,2+i));
        assert(v.forced_preserve_calls==1&&v.frames==1&&v.width==1280&&v.height==720);end(&v);
    }
    { /* Exact observed 720 transition: Main32/DPB4 to Main42/DPB1. */
        X4LiveVideo v=begin(X4_STREAM_720);fixture_explicit_dpb=4;
        n=sps_au(au,1280,720,0,77,1);au[6]=0;au[7]=32;
        assert(!x4_live_video_feed(&v,au,n,1));assert(queried.maxLevel==32);
        assert(!x4_live_video_convert_pending(&v));
        fixture_explicit_dpb=1;n=sps_au(au,1280,720,0,77,1);au[6]=0;
        unsigned before=decode_calls;
        assert(x4_live_video_feed(&v,au,n,2)==X4_SPS_CHANGED);assert(decode_calls==before);
        assert(!x4_live_video_stop(&v));assert(!live_reservations&&!active_decoder);
        assert(!x4_live_video_start(&v,pixels,capacity));
        assert(!x4_live_video_feed(&v,au,n,3));assert(queried.maxLevel==42&&queried.maxDpbFrameCount==4);
        assert(!x4_live_video_convert_pending(&v));assert(v.width==1280&&v.height==720);end(&v);
    }
    { /* Level above proven native ceiling never reaches Query/Create. */
        X4LiveVideo v=begin(X4_STREAM_720);n=sps_au(au,1280,720,0,77,1);au[7]=50;
        assert(x4_live_video_feed(&v,au,n,1)==X4_SPS_LEVEL_DPB);
        assert(!query_calls&&!create_calls&&!decode_calls);end(&v);
    }
    { /* Decoder level capability does not increase the selected720 geometry. */
        X4LiveVideo v=begin(X4_STREAM_720);n=sps_au(au,1920,1088,8,77,1);
        assert(x4_live_video_feed(&v,au,n,1)==X4_SPS_LEVEL_DPB);
        assert(!query_calls&&!create_calls&&!decode_calls);end(&v);
    }
    { /* Contract changes never reach Decode or alter the old NV12 layout. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1280,720,0,66,1);
        assert(!x4_live_video_feed(&v,au,n,1));unsigned before=decode_calls;
        n=sps_au(au,1920,1088,8,66,1);assert(x4_live_video_feed(&v,au,n,2)==X4_SPS_CHANGED);
        assert(decode_calls==before&&v.error==X4_SPS_CHANGED);
        /* The media owner may adopt a new contract only after native teardown. */
        assert(!x4_live_video_stop(&v));assert(!live_reservations&&!active_decoder);
        assert(!x4_live_video_start(&v,pixels,(size_t)1920*1080));
        assert(!x4_live_video_feed(&v,au,n,3));
        assert(!x4_live_video_convert_pending(&v));
        assert(v.width==1920&&v.height==1080&&!v.error);end(&v);
    }
    { /* Explicit DPB/ref conflict rejected before native Query/Create. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,8,66,4);
        assert(x4_live_video_feed(&v,au,n,1)<0);assert(!query_calls&&!decode_calls);end(&v);
    }
    { /* A raw slice cannot initialize the session without an SPS. */
        X4LiveVideo v=begin(X4_STREAM_1080);const uint8_t slice[]={0,0,1,0x65,0x88};
        assert(x4_live_video_feed(&v,slice,sizeof(slice),1)==X4_SPS_INVALID);assert(!query_calls);end(&v);
    }
    { /* Uncropped 1088 is not silently presented as visible1080. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,0,66,1);
        assert(x4_live_video_feed(&v,au,n,1)<0);assert(!query_calls);end(&v);
    }
    { /* Every SPS truncation fails without modifying the caller's result. */
        n=sps_au(au,1920,1088,8,100,1);size_t sps_length=n-5;
        for(size_t i=0;i<sps_length-4;++i) {
            X4StreamSps parsed={.coded_width=123};
            assert(x4_stream_sps_parse(au+4,i,&parsed)<0);assert(parsed.coded_width==123);
        }
    }
    { /* Two conflicting SPSs in the first AU never reach native Query. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1280,720,0,66,1);
        n+=sps_au(au+n,1920,1088,8,66,1);
        assert(x4_live_video_feed(&v,au,n,1)==X4_SPS_CHANGED);assert(!query_calls);end(&v);
    }
    { /* Syntax truncation cannot become a successful SPS or native call. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,8,66,1);
        assert(x4_live_video_feed(&v,au,7,1)<0);assert(!query_calls&&!decode_calls);end(&v);
    }
    { /* Oversized initial stream waits boundedly; timer runs without RTP. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,3840,2160,0,100,1);
        assert(x4_live_video_feed(&v,au,n,1)==X4_LIVE_VIDEO_WAIT_SIZE);
        assert(v.startup_waiting_size&&!v.error&&!query_calls&&!decode_calls);
        assert(x4_live_video_poll_startup(&v)==1);assert(!x4_live_video_poll_startup(&v));
        clock_us+=300000;assert(x4_live_video_poll_startup(&v)==1);
        clock_us+=10000000;assert(x4_live_video_poll_startup(&v)==X4_SPS_SIZE&&v.error);end(&v);
    }
    { /* A later admissible IDR ends initial wait and creates exactly once. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,3840,2160,0,66,1);
        assert(x4_live_video_feed(&v,au,n,1)==X4_LIVE_VIDEO_WAIT_SIZE);
        n=sps_au(au,1920,1088,8,66,1);assert(!x4_live_video_feed(&v,au,n,2));
        assert(!v.startup_waiting_size&&create_calls==1);end(&v);
    }
    for(unsigned bad=0;bad<2;++bad) {
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,8,66,1);
        bad_pointer=!bad;bad_size=bad;assert(x4_live_video_feed(&v,au,n,1)<0);assert(!v.frames);end(&v);
    }
    { /* Aggregate memory cap rejects before Create and releases only safe blocks. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,8,66,1);oversized_memory=1;
        assert(x4_live_video_feed(&v,au,n,1)<0);assert(!create_calls&&!live_reservations);end(&v);
    }
    { /* Native deletion failure retains decoder, RGB and every reservation. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,8,66,1);
        assert(!x4_live_video_feed(&v,au,n,1));unsigned owned=live_reservations;void *state=v.state;
        delete_error=-99;assert(x4_live_video_stop(&v)==-99);
        assert(v.state==state&&v.pixels==pixels&&live_reservations==owned&&active_decoder);
        delete_error=0;end(&v);
    }
    { /* Unknown GPU completion blocks native teardown and all memory release. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,8,66,1);
        assert(!x4_live_video_feed(&v,au,n,1));unsigned owned=live_reservations;
        gpu_close_error=-88;assert(x4_live_video_stop(&v)==-88);
        assert(!delete_calls&&live_reservations==owned&&v.pixels==pixels);
        gpu_close_error=0;end(&v);
    }
    { /* Success without a native handle cannot authorize reservation release. */
        X4LiveVideo v=begin(X4_STREAM_1080);n=sps_au(au,1920,1088,8,66,1);create_null=1;
        assert(x4_live_video_feed(&v,au,n,1)<0);unsigned owned=live_reservations;
        assert(x4_live_video_stop(&v)<0&&v.state&&live_reservations==owned);
        /* The fixture can prove its fake native instance was never in flight.
         * Production intentionally has no API for clearing this latch. */
        ((VideoState *)v.state)->decoder_creation_uncertain=false;active_decoder=0;end(&v);
    }
    free(pixels);puts("live_h264 native API fixture passed (no hardware emulation)");return 0;
}
