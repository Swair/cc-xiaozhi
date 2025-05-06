#pragma once
#include <opus/opus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Log.h"

namespace cill {

class OpusAudio {
public:
    OpusAudio(unsigned int sample_rate, int channels) {
        int err;
        encoder_ = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_AUDIO, &err);
        check_opus_error(err, "Failed to create Opus encoder");

        decoder_ = opus_decoder_create(sample_rate, channels, &err);
        check_opus_error(err, "Failed to create Opus decoder");
    }

    ~OpusAudio() {
        // 销毁编码器和解码器
        opus_encoder_destroy(encoder_);
        opus_decoder_destroy(decoder_);
    }

    int Encode(unsigned char* opus_data, size_t opus_size, opus_int16* pcm_data, size_t pcm_size) {
        // 编码PCM数据为Opus
        int opus_data_size;
        opus_data_size = opus_encode(encoder_, pcm_data, pcm_size, opus_data, opus_size);
        check_opus_error(opus_data_size, "Failed to encode Opus data");
        return opus_data_size;
    }

    int Decode(opus_int16* pcm_data, size_t pcm_size, unsigned char* opus_data, size_t opus_size) {
        // 编码PCM数据为Opus
        int pcm_data_size;
        pcm_data_size = opus_decode(decoder_, opus_data, opus_size, pcm_data, pcm_size, 0);
        check_opus_error(pcm_data_size, "Failed to decode Opus data");
        return pcm_data_size;
    }

public:
    // 检查Opus函数调用的返回值
    void check_opus_error(int err, const char* message) {
        if (err < 0) {
            fprintf(stderr, "%s: %s\n", message, opus_strerror(err));
            exit(EXIT_FAILURE);
        }
    }

public:
    OpusEncoder* encoder_;
    OpusDecoder* decoder_;
};

}  // namespace cill
