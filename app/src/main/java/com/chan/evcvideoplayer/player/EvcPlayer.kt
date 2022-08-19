package com.chan.evcvideoplayer.player

import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView

class EvcPlayer(): SurfaceHolder.Callback {
    private var onPreparedListener: OnPreparedListener? = null// C++层准备情况的接口
    private var onErrorListener: OnErrorListener? = null// C++层准备情况的接口
    private var onProgressListener: OnProgressListener? = null// C++层准备情况的接口
    private var dataSource: String? = null// 媒体源（文件路径， 直播地址rtmp）
    private var surfaceHolder: SurfaceHolder? = null

    companion object {
        // Used to load the 'evcvideoplayer' library on application startup.
        init {
            System.loadLibrary("native-lib")
        }
    }

    fun prepare() {
        dataSource?.let {
            prepareNative(it)
        }
    }
    /* TODO 第二节课新增 --- end */
    /**
     * set SurfaceView
     * @param surfaceView
     */
    fun setSurfaceView(surfaceView: SurfaceView) {
        if (this.surfaceHolder != null) {
            surfaceHolder?.removeCallback(this) // 清除上一次的
        }
        surfaceHolder = surfaceView.holder
        surfaceHolder?.addCallback(this) // 监听
    }

    /* TODO 第二节课新增 --- start */
    /**
     * 给jni反射调用的 准备错误了
     */
    fun onError(errorCode: Int) {
        if (null != onErrorListener) {
            onErrorListener!!.onError(errorCode)
        }
    }

    // TODO 第七节课增加 2.1
    /**
     * 给jni反射调用的
     */
    fun onProgress(progress: Int) {
        onProgressListener?.onProgress(progress)
    }


    interface OnProgressListener {
        fun onProgress(progress: Int)
    }

    /**
     * 设置准备播放时进度的监听
     */
    fun setOnOnProgressListener(onProgressListener: OnProgressListener?) {
        this.onProgressListener = onProgressListener
    }


    fun start() {
        startNative()
    }

    fun stop() {
        stopNative()
    }

    fun release() {
        releaseNative()
    }

    /**
     * 给jni反射调用的
     */
    fun onPrepared() {
        if (onPreparedListener != null) {
            onPreparedListener!!.onPrepared()
        }
    }

    fun setDataSource(dataSource: String?) {
        this.dataSource = dataSource
    }

    fun setOnPreparedListener(onPreparedListener: OnPreparedListener) {
        this.onPreparedListener = onPreparedListener
    }


    fun setOnErrorListener(onErrorListener: OnErrorListener) {
        this.onErrorListener = onErrorListener
    }

    /**
     * 准备OK的监听
     */
    interface OnPreparedListener {
        fun onPrepared()
    }

    interface OnErrorListener {
        fun onError(err_code: Int)
    }

    override fun surfaceCreated(p0: SurfaceHolder) {
    }

    override fun surfaceChanged(p0: SurfaceHolder, p1: Int, p2: Int, p3: Int) {
        setSurfaceNative(surfaceHolder!!.getSurface())
    }

    override fun surfaceDestroyed(p0: SurfaceHolder) {
    }

    fun getDuration(): Int {
        return getDurationNative()
    }

    fun seek(seekBarProgress: Int) {
        seekNative(seekBarProgress)
    }


    // TODO >>>>>>>>>>> 下面是native函数区域
    private external fun prepareNative(dataSource: String)
    private external fun startNative()
    private external fun stopNative()
    private external fun releaseNative()
    private external fun setSurfaceNative(surface: Surface) // TODO 第三节课增加的
    private external fun getDurationNative(): Int // TODO 第七节课增加 获取总时长
    private external fun seekNative(progress: Int) // TODO 第七节课增加 获取总时长


}