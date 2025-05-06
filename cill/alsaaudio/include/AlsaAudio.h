#pragma once

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <iostream>
#include <vector>

#include "FileStream.h"
#include "Log.h"

namespace cill {

// 设置终端为非规范模式，以便实时获取按键输入
void SetTerminalToNonCanonical() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

// 恢复终端为规范模式
void RestoreTerminalToCanonical() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

// 检查是否按下了空格键
bool IsSpaceKeyPressed() {
    char ch;
    INFO("wait...");
    if (read(STDIN_FILENO, &ch, 1) == 1) {
        return ch == ' ';
    }
    return false;
}

class AlsaAudio {
public:
    AlsaAudio() {}

    // 析构函数，关闭录制和播放设备
    ~AlsaAudio() {
        snd_pcm_drain(capture_handle_);
        snd_pcm_close(capture_handle_);
        snd_pcm_drain(playback_handle_);
        snd_pcm_close(playback_handle_);
    }

    void Init() {
        int err;
        // 打开录音设备
        if ((err = snd_pcm_open(&capture_handle_, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
            std::cerr << "无法打开录音 PCM 设备: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("打开录音 PCM 设备失败");
        }
        // 打开播放设备
        if ((err = snd_pcm_open(&playback_handle_, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            std::cerr << "无法打开播放 PCM 设备: " << snd_strerror(err) << std::endl;
            snd_pcm_close(capture_handle_);
            throw std::runtime_error("打开播放 PCM 设备失败");
        }
        // 分配硬件参数对象
        snd_pcm_hw_params_alloca(&hw_params_);
        SetupParams(capture_handle_);
        SetupParams(playback_handle_);
    }

    void SetConfig(unsigned int sample_rate, int frame_size, int channels, int periods, int alsa_buffer_size,
                   int alsa_period_size) {
        sample_rate_ = sample_rate;  // 20ms,  0.02*16000 = 320
        frame_size_ = frame_size;
        channels_ = channels;
        chunk_ = frame_size_ * 3;  // 20ms*3
        periods_ = periods;
        alsa_buffer_size_ = alsa_buffer_size;
        alsa_period_size_ = alsa_period_size;
    }

    bool Read(short* buffer, size_t frame_size_) {
        int err = 0;
        if ((err = snd_pcm_readi(capture_handle_, buffer, frame_size_)) !=
            static_cast<int>(frame_size_)) {
            std::cerr << "录音出错: " << snd_strerror(err) << std::endl;
            if (err == -EPIPE) {
                ERROR("录音失败EPIPE");
                if ((err = snd_pcm_prepare(capture_handle_)) < 0) {
                    std::cerr << "无法重新准备录音 PCM 设备: " << snd_strerror(err) << std::endl;
                }
                frame_size_ = snd_pcm_recover(capture_handle_, frame_size_, 0);
                if (frame_size_ < 0) {
                    std::cerr << "ALSA read error: " << snd_strerror(frame_size_) << std::endl;
                }
            } else {
                ERROR("录音失败");
            }
            return false;
        }
        return true;
    }

    bool Write(short* buffer, size_t frame_size_) {
        int err = 0;
        if ((err = snd_pcm_writei(playback_handle_, buffer, frame_size_)) !=
            static_cast<int>(frame_size_)) {
            // std::cerr << "播放出错: " << snd_strerror(err) << err << std::endl;
            if (err == -EPIPE) {
                if ((err = snd_pcm_prepare(playback_handle_)) < 0) {
                    std::cerr << "无法重新准备播放 PCM 设备: " << snd_strerror(err) << std::endl;
                    ERROR("重新准备播放 PCM 设备失败");
                    frame_size_ = snd_pcm_recover(playback_handle_, frame_size_, 0);
                } else {
                    // INFO("recover sleep");
                    usleep(100);
                }
            } else {
                ERROR("播放失败");
            }
            return false;
        }
        return true;
    }

    void Record() {
        short buffer[chunk_ * channels_];
        std::cout << "按下空格开始录音，松开空格播放录制的声音。" << std::endl;
        SetTerminalToNonCanonical();

        // 等待按下空格开始录音
        while (!IsSpaceKeyPressed()) {
            usleep(10000);
        }

        // 开始录音
        std::cout << "开始录音..." << std::endl;
        int count = 0;
        FileStream fp("abc.pcm", "wb");
        while (IsSpaceKeyPressed()) {
            INFO("begin");
            if (Read(buffer, chunk_ * channels_)) {
                audio_data_.insert(audio_data_.end(), buffer, buffer + chunk_ * channels_);
                fp.fwrite((char*)&buffer[0], 1, chunk_ * channels_ * sizeof(short));
                INFO("{}, {}", count++, audio_data_.size());
            }
        }
        fp.fclose();
        RestoreTerminalToCanonical();
        std::cout << "录音结束，开始播放..." << audio_data_.size() << std::endl;
    }

    void Play() {
        short buffer[chunk_ * channels_];

        // 开始播放录制的声音
        size_t index = 0;
        while (index < audio_data_.size()) {
            size_t frames_to_write =
                std::min(static_cast<size_t>(chunk_), (audio_data_.size() - index) / channels_);
            memcpy((char*)&buffer[0], (char*)&audio_data_[index],
                   frames_to_write * channels_ * sizeof(short));
            Write(buffer, frames_to_write);
            index += frames_to_write * channels_;
        }

        std::cout << "exit" << std::endl;
    }

private:
    // 设置 PCM 设备的硬件参数
    void SetupParams(snd_pcm_t* handle) {
        int err;
        // 填充参数对象
        if ((err = snd_pcm_hw_params_any(handle, hw_params_)) < 0) {
            std::cerr << "无法初始化硬件参数结构: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("初始化硬件参数结构失败");
        }

        // 设置参数
        if ((err = snd_pcm_hw_params_set_access(handle, hw_params_,
                                                SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            std::cerr << "无法设置访问类型: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("设置访问类型失败");
        }

        if ((err = snd_pcm_hw_params_set_format(handle, hw_params_, SND_PCM_FORMAT_S16_LE)) < 0) {
            std::cerr << "无法设置样本格式: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("设置样本格式失败");
        }

        if ((err = snd_pcm_hw_params_set_rate_near(handle, hw_params_, &sample_rate_, 0)) < 0) {
            std::cerr << "无法设置采样率: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("设置采样率失败");
        }

        if ((err = snd_pcm_hw_params_set_channels(handle, hw_params_, channels_)) < 0) {
            std::cerr << "无法设置声道数: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("设置声道数失败");
        }

        // 设置缓冲区大小
        snd_pcm_uframes_t buffer_size = alsa_buffer_size_;
        if ((err = snd_pcm_hw_params_set_buffer_size_near(handle, hw_params_, &buffer_size)) < 0) {
            std::cerr << "ALSA set buffer size error: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("设置缓冲区大小");
        }

        // 设置周期大小
        snd_pcm_uframes_t period_size = alsa_period_size_;
        if ((err = snd_pcm_hw_params_set_period_size_near(handle, hw_params_, &period_size, 0)) <
            0) {
            std::cerr << "无法设置周期大小: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("设置周期大小失败");
        }

        // 设置周期数
        if ((err = snd_pcm_hw_params_set_periods(handle, hw_params_, periods_, 0)) < 0) {
            std::cerr << "无法设置周期数: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("设置周期数失败");
        }

        // 将参数应用到 PCM 设备
        if ((err = snd_pcm_hw_params(handle, hw_params_)) < 0) {
            std::cerr << "无法设置硬件参数: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("设置硬件参数失败");
        }

        // 准备播放设备
        if ((err = snd_pcm_prepare(handle)) < 0) {
            std::cerr << "无法准备 PCM 设备: " << snd_strerror(err) << std::endl;
            throw std::runtime_error("准备播放 PCM 设备失败");
        }
    }

private:
    snd_pcm_t* capture_handle_;
    snd_pcm_t* playback_handle_;
    snd_pcm_hw_params_t* hw_params_;
    std::vector<short> audio_data_;

    unsigned int sample_rate_ = 16000;  // 20ms,  0.02*16000 = 320
    int frame_size_ = 320;
    int channels_ = 1;
    int chunk_ = frame_size_ * 3;  // 20ms*3
    int periods_ = 4;
    int alsa_buffer_size_ = 4096;
    int alsa_period_size_ = 1024;
};

}  // namespace cill