package com.chan.evcvideoplayer

import android.os.Bundle
import android.os.Environment
import android.view.View
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.chan.evcvideoplayer.databinding.ActivityMainBinding
import com.chan.evcvideoplayer.player.EvcPlayer
import java.io.File

class MainActivity : AppCompatActivity(), SeekBar.OnSeekBarChangeListener {
    var evcPlayer :EvcPlayer? = null
    var isTouch = false //用户是否拖拽了拖动条
    var duration : Int = 0;
    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        evcPlayer = EvcPlayer()
        evcPlayer?.setSurfaceView(binding.surfaceView)

        evcPlayer?.setDataSource(
            File(Environment.getExternalStorageDirectory().toString() + File.separator + "demo.mp4")
                .absolutePath
        )

        evcPlayer?.setOnPreparedListener(object :EvcPlayer.OnPreparedListener{
            override fun onPrepared() {
                //获取视频总时长
                duration = evcPlayer?.getDuration()!!

                runOnUiThread {
                    if (duration!=0){
                        //非直播
                        // duration == 119 转换成  01:59

                        // 非直播-视频
                        // tv_time.setText("00:00/" + "01:59");
                        binding.tvTime.setText("00:00/" + getMinutes(duration) + ":" + getSeconds(duration))
                        binding.tvTime.setVisibility(View.VISIBLE) // 显示
                        binding.seekBar.setVisibility(View.VISIBLE) // 显示
                    }
                    Toast.makeText(this@MainActivity, "准备成功，即将开始播放", Toast.LENGTH_SHORT).show()
                }
                evcPlayer?.start()
            }
        })

        evcPlayer?.setOnErrorListener(object : EvcPlayer.OnErrorListener {
            override fun onError(err_code: Int) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "错误码："+err_code, Toast.LENGTH_SHORT).show()
                }
                evcPlayer?.start()
            }
        })

        evcPlayer?.setOnOnProgressListener(object :EvcPlayer.OnProgressListener{
            override fun onProgress(progress: Int) {
//                binding.seekBar.max =100
//                binding.seekBar.setProgress(progress*100/duration)
                if (!isTouch){
                    //
                    runOnUiThread {
                        if (duration!=0){
                            // TODO 播放信息 动起来
                            // progress:C++层 ffmpeg获取的当前播放【时间（单位是秒 80秒都有，肯定不符合界面的显示） -> 1分20秒】
                            binding.tvTime.setText(
                                getMinutes(progress) + ":" + getSeconds(progress)
                                        + "/" +
                                        getMinutes(duration) + ":" + getSeconds(duration)
                            )
                            // TODO 拖动条 动起来 seekBar相对于总时长的百分比
                            // progress == C++层的 音频时间搓  ----> seekBar的百分比
                            // seekBar.setProgress(progress * 100 / duration 以秒计算seekBar相对总时长的百分比);
                            binding.seekBar.setProgress(progress * 100 / duration)
                        }
                    }
                }

            }
        })

        binding.seekBar.setOnSeekBarChangeListener(this)
    }

    // 119 ---> 60 59
    private fun getSeconds(duration: Int): String? { // 给我一个duration，转换成xxx秒
        val seconds = duration % 60
        return if (seconds <= 9) {
            "0$seconds"
        } else "" + seconds
    }

    // TODO >>>>>>>>>>>>>>>>>>>>>>>>>>>
    // TODO 第七节课增加 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    // 119 ---> 1.多一点点
    private fun getMinutes(duration: Int): String? { // 给我一个duration，转换成xxx分钟
        val minutes = duration / 60
        return if (minutes <= 9) {
            "0$minutes"
        } else "" + minutes
    }


    override fun onResume() {
        super.onResume()
        evcPlayer?.prepare()
    }

    override fun onStop() {
        super.onStop()
        evcPlayer?.stop()
    }

    override fun onDestroy() {
        super.onDestroy()
        evcPlayer?.release()
    }

    //拖动条
    /**
     * 是否用户拖拽
     */
    override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
        if (fromUser){
            binding.tvTime.setText(
                getMinutes(progress) + ":" + getSeconds(progress)
                        + "/" +
                        getMinutes(duration) + ":" + getSeconds(duration)
            )
        }
    }

    override fun onStartTrackingTouch(p0: SeekBar?) {
        isTouch = true
    }

    override fun onStopTrackingTouch(p0: SeekBar?) {
        isTouch = false

        //获取当前seekbar的进度
        var seekBarProgress = binding.seekBar.progress

        // SeekBar1~100  -- 转换 -->  C++播放的时间（61.546565）
        val playProgress = seekBarProgress * duration / 100

        evcPlayer?.seek(playProgress)

    }

}