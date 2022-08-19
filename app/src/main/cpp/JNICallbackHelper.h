//
// Created by Evan on 2022/8/3.
//

#ifndef EVCVIDEOPLAYER_JNICALLBACKHELPER_H
#define EVCVIDEOPLAYER_JNICALLBACKHELPER_H

#include <jni.h>

class JNICallbackHelper {

private:
    JavaVM *vm =0;
    JNIEnv *env = 0;
    jobject job;
    jmethodID jmd_prepared;
    jmethodID jmd_onError;
    jmethodID jmd_onProgress;
public:
    JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject job);


    virtual ~JNICallbackHelper();

    void onPrepared(int thread_mode);
    void onError(int thread_mode,int error_code);

    void onProgress(int thread_mode, int audio_time);
};


#endif //EVCVIDEOPLAYER_JNICALLBACKHELPER_H
