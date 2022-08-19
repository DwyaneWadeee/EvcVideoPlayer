#ifndef EVCVIDEOPLAYER_BASECHANNEL_H
#define EVCVIDEOPLAYER_BASECHANNEL_H

extern "C" {
    #include <libavcodec/avcodec.h>
#include <libavutil//time.h>

};
#include "safe_queue.h"
#include "log4c.h"
#include "JNICallbackHelper.h"

class BaseChannel{
public:
    int stream_index;
    SafeQueue<AVPacket *>packets; //压缩包 数据包
    SafeQueue<AVFrame *>frames; //原始包 数据包
    bool isPlaying;//音频 和 视频 都会有的标记 是否播放
    AVCodecContext * codecContext = 0;//音频 视频 都需要的解码器上下文

    AVRational time_base; //AudioChannel VideoChannel 都需要时间基

    JNICallbackHelper * jniCallbackHelper = 0;
    void setJNICallbakcHelper(JNICallbackHelper *helper){
        this->jniCallbackHelper = helper;
    }


    BaseChannel(int stream_index, AVCodecContext *codecContext,AVRational time_base)
            :
            stream_index(stream_index),
            codecContext(codecContext),
            time_base(time_base)//这里接受的是子类传过来的时间基
    {
        packets.setReleaseCallback(releaseAVPacket); // 给队列设置Callback，Callback释放队列里面的数据
        frames.setReleaseCallback(releaseAVFrame); // 给队列设置Callback，Callback释放队列里面的数据
    }

    ~BaseChannel() {
        packets.clear();
        frames.clear();
    }

    /**
    * 释放 队列中 所有的 AVPacket *
    * @param packet
    */
    // typedef void (*ReleaseCallback)(T *);
    static void releaseAVPacket(AVPacket ** p) {
        if (p) {
            av_packet_free(p); // 释放队列里面的 T == AVPacket
            *p = 0;
        }
    }

    /**
     * 释放 队列中 所有的 AVFrame *
     * @param packet
     */
    // typedef void (*ReleaseCallback)(T *);
    static void releaseAVFrame(AVFrame ** f) {
        if (f) {
            av_frame_free(f); // 释放队列里面的 T == AVFrame
            *f = 0;
        }
    }
};
#endif //EVCVIDEOPLAYER_BASECHANNEL_H
