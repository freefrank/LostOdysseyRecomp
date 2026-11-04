#pragma once
#define AV_INPUT_BUFFER_PADDING_SIZE 64
#define AV_CODEC_ID_XMAFRAMES 1
#define AV_SAMPLE_FMT_FLTP 8
typedef struct AVCodec { int unused; } AVCodec;
typedef struct AVCodecContext { int channels,sample_rate,thread_count; } AVCodecContext;
typedef struct AVPacket { uint8_t* data; int size; } AVPacket;
typedef struct AVFrame { uint8_t* data[2]; int nb_samples,format; } AVFrame;
inline const AVCodec* avcodec_find_decoder(int) { static AVCodec codec{}; return &codec; }
inline AVCodecContext* avcodec_alloc_context3(const AVCodec*) { return new AVCodecContext{}; }
inline void avcodec_free_context(AVCodecContext** p) { delete *p; *p=nullptr; }
inline AVPacket* av_packet_alloc() { return new AVPacket{}; }
inline void av_packet_free(AVPacket** p) { delete *p; *p=nullptr; }
inline AVFrame* av_frame_alloc() { return new AVFrame{}; }
inline void av_frame_free(AVFrame** p) { delete *p; *p=nullptr; }
inline int avcodec_open2(AVCodecContext*,const AVCodec*,void*) { return 0; }
inline int avcodec_send_packet(AVCodecContext*,const AVPacket* p) {
    sentFrames.emplace_back(p->data,p->data+p->size); return 0;
}
inline int avcodec_receive_frame(AVCodecContext*,AVFrame* p) {
    static float samples[512]{};
    p->data[0]=p->data[1]=reinterpret_cast<uint8_t*>(samples);
    p->nb_samples=512; p->format=AV_SAMPLE_FMT_FLTP; return 0;
}
