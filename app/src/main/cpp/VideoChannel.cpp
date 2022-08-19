//
// Created by Evan on 2022/8/3.
//

#include "VideoChannel.h"
#include "EvcPlayer.h"

/**
 * 原始包比较简单，因为不需要考虑关键帧
 * @param q
 */
void dropAVFrame(queue<AVFrame *> &q){
    if (!q.empty()){
        AVFrame *frame = q.front();
        BaseChannel::releaseAVFrame(&frame);
        q.pop();
    }
}

/**
 * 要考虑关键帧
 * @param q
 */
void dropAVPacket(queue<AVPacket *> &q){
    while  (!q.empty()){
        AVPacket *pkt = q.front();
        if (pkt->flags!=AV_PKT_FLAG_KEY){//如果不是关键帧
            BaseChannel::releaseAVPacket(&pkt);
            q.pop();
        } else{
            break;
        }
    }
}

VideoChannel::VideoChannel(int stream_index, AVCodecContext *codecContext, AVRational rational,
                           int fps)
        : BaseChannel(stream_index, codecContext, rational),
        fps(fps){
    frames.setSyncCallback(dropAVFrame);
    packets.setSyncCallback(dropAVPacket);
};

VideoChannel::~VideoChannel() {
}

void VideoChannel::stop() {

}

void *task_video_play(void *args) {
    auto *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_play();
    return 0;
}
void *task_video_decode(void *args) {
    auto *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_decode();
    return 0;
}

void VideoChannel::video_decode() {
    AVPacket *pkt = nullptr;
    while (isPlaying){
        //内存泄漏点
        if (isPlaying&&frames.size()>100){
            av_usleep(10*1000);
            continue;
        }
        //拿 压缩包
        int ret = packets.getQueueAndDel(pkt);

//        LOGD("evcplayer3");

        if (!isPlaying){
//            LOGD("evcplayer4");

            break;//如果关闭播放，跳出循环。
        }

        if(!ret){//ret == 0
//            LOGD("evcplayer5");

            continue;//哪怕没有成功，也要继续
            //有可能压缩包取得太慢了，等一等
        }

        //新版本写法：1、先发送pkt给缓冲区，2、从缓冲区取出原始包。
        //发到缓冲区
//        LOGD("evcplayer::::::-3");

        ret = avcodec_send_packet(codecContext,pkt);
//        LOGD("evcplayer::::::-4");

        //释放pkt
//        releaseAVPacket(&pkt);//放后面去释放

        //拿原始包
        if (ret){
            break;//avcodec_send_packet 出现了错误
        }

        //从缓冲区获取原始包，拿frame
        AVFrame * frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext,frame);
        if (ret == AVERROR(EAGAIN)){
            //B帧 B帧参考前面 B帧参考后面失败 有可能P帧没有出来，再重新拿一次就行，直到拿到P为止
            continue;
        } else if (ret!=0){
            //内存泄露：解码的frame出错，马上释放，防止在堆开辟
            if (frame){
                releaseAVFrame(&frame);
            }
            break;//错误了
        }
        //终于拿到了原始包
        //视频 插入::::原始包::::::
        frames.insertToQueue(frame);
        //在这释放pkg本身空间 和 pkg成员指向的空间释放
        av_packet_unref(pkt);//释放子的引用
        releaseAVPacket(&pkt);//释放

//        LOGD("evcplayer::::::-2");

    }//end while
    av_packet_unref(pkt);
    releaseAVPacket(&pkt);
}

void VideoChannel::video_play() {
    AVFrame *frame = 0;
    uint8_t *dst_data[4];//RGBA
    int dst_linesize[4];//RGBA

    //给 dst_data 申请内存   width * height * 4 xxxx
    av_image_alloc(dst_data, dst_linesize,
                   codecContext->width,
                   codecContext->height,
                   AV_PIX_FMT_RGBA, 1);

    //格式转换
    //原始包(YUV)----Android屏幕(RGBA数据)
    //使用 libswscale模块
    SwsContext * sws_ctx = sws_getContext(
            //下面是输入环节
            codecContext->width,
            codecContext->height,
            codecContext->pix_fmt,//自动获取文件的像素格式， AVPixelFormat(AV_PIX_FMT_YUV420P)//这种方法是写死的、
            //下面是输出环节
            codecContext->width,
            codecContext->height,
            AV_PIX_FMT_RGBA,//目标格式
            //选择转换的算法
            SWS_BILINEAR,
            //后面的参数不需要用到，后面是滤镜效果等等的，不需要
            NULL,NULL,NULL
            );

    while (isPlaying){
//        LOGD("evcplayer-1");
        //拿 原始包
        int ret = frames.getQueueAndDel(frame);
//        LOGD("evcplayer0");

        if (!isPlaying){
//            LOGD("evcplayer1");
            break;//如果关闭播放，跳出循环。
        }

        if(!ret){//ret == 0
//            LOGD("evcplayer2");
            continue;//哪怕没有成功，也要继续
            //有可能原始包生产得太慢了，等一等
        }
//        (YUV)---->(RGBA数据)
        sws_scale(sws_ctx,

                  //原始包的数据,一行的数据
                  frame->data,
                  //原始包数据的大小，一行的大小
                  frame->linesize,
                  0,
                  codecContext->height,
                  //上面还是yuv的
                  //下面是RGBA的成果

                  //下面是输出环节
                  dst_data,
                  dst_linesize
                  );

        //音视频同步，根据fps来休眠
        //加入fps间隔时间
        double extra_delay = frame->repeat_pict/(2*fps);
        double fps_delay = 1/fps; //根据fps得到延迟时间
        double real_delay = fps_delay = extra_delay;//当前帧的延时时间

        //fps间隔件后的效果，任何播放器都会有
        //不能用：因为根据视频的fps延时吃力，和音频没有任何关系
//        av_usleep(real_delay*1000000);

        //下面是音视频同步
        double video_time = frame->best_effort_timestamp* av_q2d(time_base);
        double audio_time = audio_channel->audio_time;

        //判断两个时间差值，你追我赶
        double time_diff = video_time - audio_time;

        if(time_diff>0){
            //视频等音频[睡眠]
            if (time_diff>1){
                //说明差距很大
                //我不会睡很久，就稍微睡一下
                av_usleep((real_delay*2)*1000000);
            } else{
                //说明差值不大
                av_usleep((real_delay+time_diff)*1000000);
            }
        }
        if (time_diff<0){
            //所以控制视频播放快一点，丢帧，i帧是绝对不能丢的

            if (fabs(time_diff)<=0.05){
                //多线程，同步丢包
                frames.sync();
                continue;//丢完去取下一个
            }

        } else{
            //百分百同步，基本上不存在

        }



        //ANativeWindow 渲染工作
        //SurfaceView --- ANativeWindows

        //渲染
        //如何渲染一帧图像？ 答案：需要宽，高，数据

        //我拿不到Surface，只能回调给native-lib.cpp
        //基础：数据被传递会退化为指针,所以第一个参数只是数组，所以第四个参数是数组的大小
        //传dst_data 跟 dst_data[0]无异 翻车了，报错
        renderCallback(dst_data[0],codecContext->width,codecContext->height,dst_linesize[0]);

        av_frame_unref(frame);//内存泄露
        releaseAVFrame(&frame);//释放原始包，已经渲染完了，没用了。
    }
    av_frame_unref(frame);//内存泄露
    releaseAVFrame(&frame);//出现错误，所推出的循环，都要释放
    isPlaying=0;
    av_free(&dst_data[0]);
    sws_freeContext(sws_ctx);
}

void VideoChannel::start() {
    isPlaying = true;

    //队列开始工作
    packets.setWork(1);
    frames.setWork(1);

    //第一个线程：取出队列的压缩包进行编码 编码后的原始包 再push队列中去  视频：YUV
    pthread_create(&pid_video_decode,nullptr,task_video_decode,this);
    //第二个线程：取出原始包，播放
    pthread_create(&pid_video_play,nullptr,task_video_play,this);

    //为什么要这样分开两个线程?网路很卡，我也可以缓存播放

}

void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

void VideoChannel::setAudioChannel(AudioChannel *audio_channel) {
    this->audio_channel = audio_channel;
}



