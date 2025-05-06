#pragma once
#include <stdio.h>

#include <string>
#include <vector>

#include "Log.h"

#define ASSERT(expression)                                                                        \
    if (!(expression)) {                                                                          \
        ERROR("ASSERT `{}' failed. [{}, {}({})]", #expression, __FUNCTION__, __FILE__, __LINE__); \
        assert(expression);                                                                       \
    }

namespace cill {

struct WAVE_HEADER {
    char chunkID[4];         // 内容为"RIFF"
    unsigned int chunkSize;  // 最后填写，WAVE格式音频的大小
    char format[4];          // 内容为"WAVE"
};

struct WAVE_FMT {
    char subchunk1ID[4];         // 内容为"fmt "
    unsigned int subchunk1Size;  // 内容为wavFmt占的字节数，为16
    short int audioFormat;       // 如果为PCM，改值为 1
    short int numChannels;       // 通道数，单通道=1，双通道=2
    unsigned int sampleRate;     // 采样频率
    unsigned int byteRate;       /* ==sampleRate*numChannels*bitsPerSample/8 */
    short int blockAlign;        //==numChannels*bitsPerSample/8
    short int bitsPerSample;     // 每个采样点的bit数，8bits=8, 16bits=16
};

struct WAVE_DATA {
    char subchunk2ID[4];         // 内容为"data"
    unsigned int subchunk2Size;  //==NumSamples*numChannels*bitsPerSample/8
};

class FileStream {
public:
    FileStream();
    FileStream(const std::string& filePath, const std::string& FLAG);
    ~FileStream();
    int reset(const std::string& filePath, const std::string& FLAG);
    int fopen(const std::string& filePath, const std::string& FLAG);
    int valid();
    int fwrite(void* buf, int typesize, int len);
    int fread(void* buf, int typesize, int len);
    int fseek(int offset, int FLAG);
    int ftell();
    void rewind();
    void fclose();
    std::vector<char> readStream(size_t begin_offset, size_t end_offset);
    std::vector<char> readStream();
    std::string readAll();
    int wavfopen(const std::string& filePath, const std::string& FLAG);
    void wavfclose(int pcmsize, int channels);
    void saveWavWithOneChannel(const std::string& path, const std::vector<char>& src);
    void saveWavWithTwoChannel(const std::string& path, const std::vector<char>& first,
                               std::vector<char>& second);

private:
    FILE* fp_ = nullptr;
    std::string filePath_;
};

bool pcm2wav(const std::string& wavFilePath, const std::string& pcmFilePath);
bool wav2pcm(const std::string& pcmFilePath, const std::string& wavFilePath);
void wav2mp3(const std::string& dst_path, const std::string& src_path, bool override);

}  // namespace cill