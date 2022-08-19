//
// Created by Evan on 2022/8/3.
//

#include "JNICallbackHelper.h"
#include "util.h"

JNICallbackHelper::JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject job) {
    this->vm = vm;
    this->env = env;
//    this->job = job; //坑：jobject不能跨越线程，不能跨越函数，必须全局引用
    this->job = env->NewGlobalRef(job);//提示全局引用

//env也不能跨线程


    jclass clazz = env->GetObjectClass(job);
    jmd_prepared = env->GetMethodID(clazz,"onPrepared","()V");
    jmd_onError = env->GetMethodID(clazz,"onError","(I)V");
    //播放回调
    jmd_onProgress = env->GetMethodID(clazz,"onProgress","(I)V");
}

JNICallbackHelper::~JNICallbackHelper() {
    vm = 0;
    env->DeleteGlobalRef(job);
    job = 0;
    env = 0;
}

void JNICallbackHelper::onPrepared(int thread_mode) {
    if (thread_mode == THREAD_MAIN){
        env->CallVoidMethod(job,jmd_prepared);
    }else if (thread_mode == THREAD_CHILD){
        //子线程
        //env也不能跨线程   全新的env
        JNIEnv * env_child;
        vm->AttachCurrentThread(&env_child,0);

        env_child->CallVoidMethod(job,jmd_prepared);

        vm->DetachCurrentThread();

    }
}

void JNICallbackHelper::onError(int thread_mode, int error_code)  {
    if (thread_mode == THREAD_MAIN){
        env->CallVoidMethod(job,jmd_onError,error_code);
    }else if (thread_mode == THREAD_CHILD){
        //子线程
        //env也不能跨线程   全新的env
        JNIEnv * env_child;
        vm->AttachCurrentThread(&env_child,0);

        env_child->CallVoidMethod(job,jmd_onError,error_code);

        vm->DetachCurrentThread();

    }
}

void JNICallbackHelper::onProgress(int thread_mode, int audio_time) {
    if (thread_mode == THREAD_MAIN){
        env->CallVoidMethod(job,jmd_onProgress,audio_time);
    }else if (thread_mode == THREAD_CHILD){
        //子线程
        //env也不能跨线程   全新的env
        JNIEnv * env_child;
        vm->AttachCurrentThread(&env_child,0);

        env_child->CallVoidMethod(job,jmd_onProgress,audio_time);

        vm->DetachCurrentThread();

    }
}

