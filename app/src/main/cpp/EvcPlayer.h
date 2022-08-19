//
// Created by Evan on 2022/8/3.
//

#ifndef EVCVIDEOPLAYER_EVCPLAYER_H
#define EVCVIDEOPLAYER_EVCPLAYER_H

#include <cstring>
#include <pthread.h>
#include "AudioChannel.h"
#include "VideoChannel.h"
#include "JNICallbackHelper.h"
#include "util.h"


extern "C" { //ffmpeg是存C写的，必须采用c的编译方式，否则崩溃
#include <libavformat/avformat.h>
#include <libavutil//time.h>
};

#include <android/log.h>
#define LOG_TAG  "C_TAG"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)


class EvcPlayer {


private:
    char *data_source = 0;
    pthread_t pid_prepare;
    pthread_t pid_start;
    AVFormatContext *formatContext = 0 ;

    AudioChannel * audio_channel = 0;
    VideoChannel * video_channel = 0;

    JNICallbackHelper *helper = 0;
    bool isPlaying;
    RenderCallback renderCallback;
    int duration;

    pthread_mutex_t seek_mutex;
    pthread_t pid_stop;

public:
    EvcPlayer(const char *data_source, JNICallbackHelper *pHelper);

    ~EvcPlayer();

    void prepare();

    void prepare_();

    void start();

    void start_();

    void setRenderCallback(RenderCallback renderCallback);

    int getDuration();

    void seek(jint play_value);

    void stop();

    void stop_(EvcPlayer *pPlayer);
};


#endif //EVCVIDEOPLAYER_EVCPLAYER_H
