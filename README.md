# EvcVideoPlayer
基于ffmpeg+rtmp的播放器，支持网络直播+本地视频播放
音频部分使用的是OpenSL ES
##RTMP与FFmpeg合并混编
可以参考：
https://blog.csdn.net/u014078003/article/details/125221733

##如何使用：
需要在MainActivity中设置播放源，支持Rtmp直播和播放本地视频。


![流程](https://raw.githubusercontent.com/DwyaneWadeee/image/main/EvcPlayerFLow.jpg)

##关于音视频同步：

以音频播放时间戳为准
获取音视频分别的时间，判断哪一个在前面：
视频>音频：视频睡眠等待音频
视频<音频：视频丢帧，赶上音频（不能丢I帧）
