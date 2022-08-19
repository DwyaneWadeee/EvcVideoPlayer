//
// Created by Evan on 2022/8/3.
//

#include "EvcPlayer.h"

EvcPlayer::EvcPlayer(const char *data_source, JNICallbackHelper *pHelper) {
//    this->data_source = data_source;
    // 如果被释放，会造成悬空指针

    // 深拷贝
//    this->data_source = new char[strlen(data_source)];
    // Java: demo.mp4
    // C层：demo.mp4\0  C层会自动 + \0,  strlen不计算\0的长度，所以我们需要手动加 \0

    this->data_source = new char[strlen(data_source) + 1];
    strcpy(this->data_source, data_source); // 把源 Copy给成员

    this->helper = pHelper;

    pthread_mutex_init(&seek_mutex, nullptr);
}

EvcPlayer::~EvcPlayer() {
    if (data_source) {
        delete data_source;
        data_source = nullptr;
    }

    if (helper) {
        delete helper;
        helper = nullptr;
    }
    pthread_mutex_destroy(&seek_mutex);
}


void *task_prepare(void *args) {//这个函数跟Evcplayer这个对象没有关系，没法拿他的私有成员
//    avformat_open_input(0,this->data_);
    auto *player = static_cast<EvcPlayer *>(args);
    player->prepare_();
    return nullptr;//必须返回，错误很难找
}

void *task_start(void *args) {
    auto *player = static_cast<EvcPlayer *>(args);
    player->start_();
    return nullptr;//必须返回，错误很难找
}


void *task_stop(void *args) {
    auto *player = static_cast<EvcPlayer *>(args);
    player->stop_(player);
    return nullptr; // 必须返回，坑，错误很难找
}

void EvcPlayer::prepare_() {
    formatContext = avformat_alloc_context();

    AVDictionary *dictionary = nullptr;
    av_dict_set(&dictionary, "timeout", "5000000", 0);
    //第一步，打开媒体地址（文件路径， 直播地址rtmp）
    int r = avformat_open_input(&formatContext, data_source, nullptr, &dictionary);

    //释放字典
    av_dict_free(&dictionary);
    if (r) {
        //错误
        helper->onError(THREAD_CHILD, 1);
        avformat_close_input(&formatContext);
        return;
    }

    //第二部，查找媒体中音视频流的信息
    r = avformat_find_stream_info(formatContext, nullptr);
    if ((r < 0)) {
        //错误
        helper->onError(THREAD_CHILD, 2);
        avformat_close_input(&formatContext);
        return;
    }

    //获取总时长
    this->duration = formatContext->duration / AV_TIME_BASE;//单位是时间基

    AVCodecContext *codecContext = nullptr;
    //第三部：根据流的信息，流的个数，用循环来查找
    for (int steam_index = 0; steam_index < formatContext->nb_streams; ++steam_index) {
        //第四部，获取媒体流（视频还是音频）
        AVStream *stream = formatContext->streams[steam_index];
        //第五步，从上面的流中获取解码的参数，由于后面的编码器 解码器都需要参数[宽高]
        AVCodecParameters *parameters = stream->codecpar;
        //第六步，根据上面的参数获取解码器
        AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
        if(!codec){
            if(helper){
                helper->onError(THREAD_CHILD, 25);
            }
            avformat_close_input(&formatContext);
        }
        //第七部,编解码器上下文（这个才是真正干活的）
        codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            //错误
            helper->onError(THREAD_CHILD, 3);
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }
        //第八步，他目前是一张白纸
        r = avcodec_parameters_to_context(codecContext, parameters);
        if (r < 0) {
            //错误
            helper->onError(THREAD_CHILD, 4);
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        //第九步，打开解码器
        r = avcodec_open2(codecContext, codec, nullptr);
        if (r) {
            //错误
            helper->onError(THREAD_CHILD, 5);
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        //音视频同步
        AVRational time_base = stream->time_base;

        //第十部，从编解码器参数中，获取流的类型
        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) {
            audio_channel = new AudioChannel(steam_index, codecContext, time_base);

            if (this->duration != 0) {
                //非直播才有意义传helper
                audio_channel->setJNICallbakcHelper(helper);

            }
        } else if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {

            if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                //虽然是视频流，但是只有一帧封面
                continue;
            }
            AVRational fps_rational = stream->avg_frame_rate;
            int fps = av_q2d(fps_rational);

            video_channel = new VideoChannel(steam_index, codecContext, time_base, fps);
            video_channel->setRenderCallback(renderCallback);

            if (this->duration != 0) {
                //非直播才有意义传helper
                video_channel->setJNICallbakcHelper(helper);
            }
        }
    }

    //第11步，如果流中没有视频跟音频流，【健壮性校验】
    if (!audio_channel && !video_channel) {
        //错误
        helper->onError(THREAD_CHILD, 6);
        if(codecContext){
            avcodec_free_context(&codecContext);
        }
        avformat_close_input(&formatContext);
        return;
    }

    //第12步，恭喜你，准备成功，通知上层
    if (helper) {
        helper->onPrepared(THREAD_CHILD);
    }
//    this->data_source
}

void EvcPlayer::prepare() {
    pthread_create(&pid_prepare, 0, task_prepare, this);
}


void EvcPlayer::start_() {
    while (isPlaying) {
        //解决方案：我不丢弃，等待队列中的数据消费
        if (video_channel && video_channel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }

        if (audio_channel && audio_channel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        //AVPacket 可能是音频，可能是视频（压缩包）
        AVPacket *packet = av_packet_alloc();
        int ret = av_read_frame(formatContext, packet);
//        LOGD("evcplayer7");

        if (!ret) {//ret==0   ；0 if OK, < 0 on error or end of file

            // AudioChannel    队列
            // VideioChannel   队列

            //把AVPacket*加入队列， 音频 和 视频
            /*AudioChannel.insert(packet);
            VideioChannel.insert(packet);*/


            //内存泄露关键点（控制packet队列大小，等待队列中的数据被消费）
            //区分音视频包放入不同的队列
            if (video_channel && video_channel->stream_index == packet->stream_index) {
                //视频 插入::::压缩包::::::
                video_channel->setAudioChannel(audio_channel);
                video_channel->packets.insertToQueue(packet);
//                LOGD("evcplayer8");
            } else if (audio_channel && audio_channel->stream_index == packet->stream_index) {
                //音频 插入::::压缩包::::::
                audio_channel->packets.insertToQueue(packet);
            }
        } else if (ret == AVERROR_EOF) {//读到文件末尾了
            //文件读完了，并不代表播放完毕，
//            LOGD("evcplayer9");
            //内存泄露
            if (video_channel->packets.empty() && audio_channel->packets.empty()) {
                break;
            }
            //todo
        } else {
            //错误
//            LOGD("evcplayer10");
            break;
        }
    }
    isPlaying = 0;
    video_channel->stop();
    audio_channel->stop();
}

void EvcPlayer::start() {
    isPlaying = 1;

    if (video_channel) {
        video_channel->start();
    }

    if (audio_channel) {
        audio_channel->start();
    }

    //把压缩包 加入到队列里去，不区分音视频
    pthread_create(&pid_start, 0, task_start, this);
}

void EvcPlayer::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

int EvcPlayer::getDuration() {
    return duration;
}

void EvcPlayer::seek(jint progress) {

    //安全问题 多线程
    // 健壮性判断
    if (progress < 0 || progress > duration) {
        // TODO 同学们自己去完成，给Java的回调
        return;
    }
    if (!audio_channel && !video_channel) {
        // TODO 同学们自己去完成，给Java的回调
        return;
    }
    if (!formatContext) {
        // TODO 同学们自己去完成，给Java的回调
        return;
    }

    pthread_mutex_lock(&seek_mutex);

    //安全问题
    //-1为默认，ffmpeg自动找视频还是音频，
    // AVSEEK_FLAG_FRAME 找关键帧
    int r = av_seek_frame(formatContext,-1,
                  progress*AV_TIME_BASE,AVSEEK_FLAG_FRAME);
    if (r<0){
        //自己完成，给java回调
        return;
    }

    //正在播放，用户seek，停止播放
    if (audio_channel){
        audio_channel->packets.setWork(0);
        audio_channel->frames.setWork(0);
        audio_channel->packets.clear();
        audio_channel->frames.clear();
        audio_channel->packets.setWork(1);
        audio_channel->frames.setWork(1);
    }

    if (video_channel){
        video_channel->packets.setWork(0);
        video_channel->frames.setWork(0);
        video_channel->packets.clear();
        video_channel->frames.clear();
        video_channel->packets.setWork(1);
        video_channel->frames.setWork(1);
    }


    pthread_mutex_unlock(&seek_mutex);

}

void EvcPlayer::stop() {
    //只要用户关闭了，就不准Java层start 播放
    helper = nullptr;
    if (audio_channel) {
        audio_channel->jniCallbackHelper = nullptr;
    }
    if (video_channel) {
        video_channel->jniCallbackHelper = nullptr;
    }

    //两个线程还在跑，
    // 要稳稳地停下来
    // 再释放DerryPlaer所有工作
    // 等待会导致ANR
    //
    // 所以再开线程去释放两个线程
    pthread_create(&pid_stop, nullptr, task_stop, this);

}

void EvcPlayer::stop_(EvcPlayer *player) {
    isPlaying = false;
    pthread_join(pid_prepare, nullptr);
    pthread_join(pid_start, nullptr);

    // pid_prepare pid_start 就全部停止下来了  稳稳的停下来
    if (formatContext) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }
    DELETE(audio_channel);
    DELETE(video_channel);
    DELETE(player);
}


