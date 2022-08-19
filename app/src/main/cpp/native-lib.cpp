#include <jni.h>
#include <string>
#include "EvcPlayer.h"
#include "JNICallbackHelper.h"
#include <android/native_window_jni.h>


EvcPlayer *player=0;
JavaVM *vm = 0;
ANativeWindow *window =0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;//静态初始化


jint JNI_OnLoad(JavaVM *vm, void *args) {
    ::vm = vm;
    return JNI_VERSION_1_6;
}

//函数指针实现
void renderFrame(uint8_t * src_data,int width,int height,int src_lineSize){
    //渲染工作
    //先锁住
    pthread_mutex_lock(&mutex);
    if (!window){
        pthread_mutex_unlock(&mutex);//出现了必须考虑到释放锁，防止死锁

    }

    ANativeWindow_setBuffersGeometry(window,width,height,WINDOW_FORMAT_RGBA_8888);

    //他自己有个缓冲区 buffer
    ANativeWindow_Buffer window_buffer;
    if (ANativeWindow_lock(window,&window_buffer,0)){
        ANativeWindow_release(window);
        window = 0;

        pthread_mutex_unlock(&mutex);//解锁，怕出现死锁
        return;
    }

    //开始真正渲染，因为window没有被锁住了，就可以把rgba数据--》字节对齐
    //填充buffer， 画面就出来了。
    uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
    int dst_linesize = window_buffer.stride * 4;

    for (int i = 0; i < window_buffer.height; ++i) {//一行一行显示
        //视频分辨率：426*240
        //426*4(rgba8888) = 1704
//        memcpy(dst_data +i*1704,src_data+i*1704,1704);//花屏，崩溃
        //ANativeWindow_Buffer 16字节对齐算法， 1704无法以64位字节对齐
//        memcpy(dst_data + i * 1792, src_data + i * 1704, 1792); // OK的
        //  通用写法
        memcpy(dst_data + i * dst_linesize, src_data + i * src_lineSize, dst_linesize); // OK的

    }
    //数据刷新
    ANativeWindow_unlockAndPost(window);

    //解锁
    pthread_mutex_unlock(&mutex);
}


extern "C"
JNIEXPORT void JNICALL
Java_com_chan_evcvideoplayer_player_EvcPlayer_prepareNative(JNIEnv *env, jobject thiz, jstring data_source) {
    const char * data_source_  = env->GetStringUTFChars(data_source,0);
    JNICallbackHelper *helper = new JNICallbackHelper(vm,env,thiz);
    player = new EvcPlayer(data_source_, helper);
    player->setRenderCallback(renderFrame);
    player->prepare();
    env->ReleaseStringUTFChars(data_source,data_source_);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_chan_evcvideoplayer_player_EvcPlayer_startNative(JNIEnv *env, jobject thiz) {
    if (player){
        player->start();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_chan_evcvideoplayer_player_EvcPlayer_stopNative(JNIEnv *env, jobject thiz) {
    if(player){
        player->stop();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_chan_evcvideoplayer_player_EvcPlayer_releaseNative(JNIEnv *env, jobject thiz) {
    pthread_mutex_lock(&mutex);

    //先释放之前的显示窗口
    if (window){
        ANativeWindow_release(window);
        window = nullptr ;
    }
    //解锁
    pthread_mutex_unlock(&mutex);

    DELETE(player);
    DELETE(vm);
    DELETE(window);
}

/**
 * 实例化出window
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_chan_evcvideoplayer_player_EvcPlayer_setSurfaceNative(JNIEnv *env, jobject thiz,jobject surface){
    //多线程
    //先锁住
    pthread_mutex_lock(&mutex);

    //先释放之前的显示窗口
    if (window){
        ANativeWindow_release(window);
        window = nullptr ;
    }

    //创建新的窗口用于视频显示
    window = ANativeWindow_fromSurface(env,surface);

    //解锁
    pthread_mutex_unlock(&mutex);

}

extern "C"
JNIEXPORT jint JNICALL
Java_com_chan_evcvideoplayer_player_EvcPlayer_getDurationNative(JNIEnv *env, jobject thiz) {
    if (player) {
        return player->getDuration();
    }
     return 0;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_chan_evcvideoplayer_player_EvcPlayer_seekNative(JNIEnv *env, jobject thiz, jint progress) {
    if(player){
        player->seek(progress);
    }
}