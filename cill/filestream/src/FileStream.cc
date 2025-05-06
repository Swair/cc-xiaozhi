#include "FileStream.h"

namespace cill {

FileStream::FileStream() {}
FileStream::FileStream(const std::string& filePath, const std::string& FLAG) {
    fopen(filePath, FLAG);
}

FileStream::~FileStream() { fclose(); }

int FileStream::reset(const std::string& filePath, const std::string& FLAG) {
    fclose();
    fopen(filePath, FLAG);
    return 0;
}

int FileStream::fopen(const std::string& filePath, const std::string& FLAG) {
    if (!filePath.size()) {
        ERROR("FileStream::fopen, filePath {} is null", filePath);
        return -1;
    }

    filePath_ = filePath;
    fp_ = ::fopen(filePath.c_str(), FLAG.c_str());
    // INFO(NOTING, "fopen, fp_(%x, %s)", fp_, filePath_.c_str());
    if (nullptr == fp_) {
        ERROR("FileStream::fopen, fopen {} is failed", filePath);
        return -1;
    }
    return 0;
}

int FileStream::valid() {
    if (nullptr != fp_)
        return 1;
    else
        return 0;
}

int FileStream::fwrite(void* buf, int typesize, int len) {
    if (nullptr != fp_)
        return ::fwrite(buf, typesize, len, fp_);
    else
        return -1;
}

int FileStream::fread(void* buf, int typesize, int len) {
    if (nullptr != fp_) {
        return ::fread(buf, typesize, len, fp_);
    } else {
        return -1;
    }
}

int FileStream::fseek(int offset, int FLAG) { return ::fseek(fp_, offset, FLAG); }
int FileStream::ftell() { return ::ftell(fp_); }
void FileStream::rewind() { ::rewind(fp_); }
void FileStream::fclose() {
    if (nullptr != fp_) {
        ::fclose(fp_);
        fp_ = nullptr;
    }
}

/*  0L          ~   begin_offset
    end_offset  ~   SEEK_END
*/
std::vector<char> FileStream::readStream(size_t begin_offset, size_t end_offset) {
    fseek(0, SEEK_END);
    size_t file_size = ftell();
    if (file_size < 0) {
        ERROR("the content of {} is wrong", filePath_);
        return {};
    }

    size_t rline = file_size - begin_offset - end_offset;

    std::vector<char> rawbuf(rline);
    // rewind before read
    rewind();
    fseek(begin_offset, 0L);
    size_t nlen = fread(&rawbuf[0], 1, rline);
    ASSERT(nlen == rline);
    return rawbuf;
}

std::vector<char> FileStream::readStream() {
    fseek(0, SEEK_END);
    size_t len = ftell();
    if (len < 0) {
        ERROR("the content of {} is wrong", filePath_);
        return {};
    }

    std::vector<char> rawbuf(len);
    // rewind before read
    rewind();
    size_t nlen = fread(&rawbuf[0], 1, len);
    ASSERT(nlen == len);
    return rawbuf;
}

std::string FileStream::readAll() {
    fseek(0, SEEK_END);
    auto len = ftell();
    if (len < 0) {
        ERROR("the content of {} is wrong", filePath_);
        throw -1;
    }

    std::string lines(len, '0');
    int size = fread(&lines[0], 1, len);

    if (size < 0) lines = "";

    return lines;
}

int FileStream::wavfopen(const std::string& filePath, const std::string& FLAG) {
    fopen(filePath, FLAG);
    size_t wave_header_size = sizeof(WAVE_HEADER) + sizeof(WAVE_FMT) + sizeof(WAVE_DATA);
    ASSERT(wave_header_size == 44);
    fseek(wave_header_size, 0L);
    return 0;
}

void FileStream::wavfclose(int pcmsize, int channels) {
    WAVE_HEADER wavHeader;
    WAVE_FMT wavFmt;
    WAVE_DATA wavData;

    /* write WAVE_HEADER */
    rewind();
    memcpy(wavHeader.chunkID, "RIFF", 4);
    wavHeader.chunkSize = sizeof(wavHeader) + sizeof(wavFmt) + pcmsize;
    memcpy(wavHeader.format, "WAVE", 4);

    fwrite(&wavHeader, sizeof(wavHeader), 1);

    /* write wavFmt */
    memcpy(wavFmt.subchunk1ID, "fmt ", 4);
    wavFmt.subchunk1Size = 16;
    wavFmt.audioFormat = 0x0001;
    wavFmt.numChannels = channels;  // channel
    wavFmt.sampleRate = 8000;       // samplerate
    wavFmt.bitsPerSample = 16;
    wavFmt.byteRate = wavFmt.sampleRate * wavFmt.numChannels * wavFmt.bitsPerSample / 8;
    wavFmt.blockAlign = wavFmt.numChannels * wavFmt.bitsPerSample / 8;
    fwrite(&wavFmt, sizeof(wavFmt), 1);

    /* wirte wavData */
    memcpy(wavData.subchunk2ID, "data", 4);
    wavData.subchunk2Size = pcmsize;
    fwrite(&wavData, sizeof(wavData), 1);

    fclose();
}

void FileStream::saveWavWithOneChannel(const std::string& path, const std::vector<char>& src) {
    wavfopen(path, "wb");
    fwrite((char*)&src[0], 1, src.size());
    wavfclose(src.size(), 1);
}

void FileStream::saveWavWithTwoChannel(const std::string& path, const std::vector<char>& first,
                                       std::vector<char>& second) {
    wavfopen(path, "wb");
    size_t len1 = first.size();
    size_t len2 = second.size();
    size_t len = len1 > len2 ? len1 : len2;
    size_t recordSize = 2 * len;
    std::vector<char> dst(recordSize, 0);

    size_t ci = 0;
    size_t si = 0;
    size_t i = 0;
    while (i < recordSize) {
        if (ci < len) {
            dst[i] = first[ci];
            dst[i + 1] = first[ci + 1];
            ci += 2;
        } else {
            dst[i] = 0;
            dst[i + 1] = 0;
        }

        if (si < len) {
            dst[i + 2] = second[si];
            dst[i + 3] = second[si + 1];
            si += 2;
        } else {
            dst[i + 2] = 0;
            dst[i + 3] = 0;
        }
        i += 4;
    }
    fwrite((char*)&dst[0], 1, dst.size());
    wavfclose(dst.size(), 2);
}

bool pcm2wav(const std::string& wavFilePath, const std::string& pcmFilePath) {
    if (wavFilePath.size() == 0 || pcmFilePath.size() == 0) {
        INFO("error in pcm2wav");
        return false;
    }
    FileStream pcmfs(pcmFilePath.c_str(), "rb");
    FileStream wavfs;
    wavfs.wavfopen(wavFilePath.c_str(), "wb");
    std::vector<char> buf = pcmfs.readStream();
    wavfs.fwrite(&buf[0], 1, buf.size());
    wavfs.wavfclose(buf.size(), 1);
    return true;
}

bool wav2pcm(const std::string& pcmFilePath, const std::string& wavFilePath) {
    if (wavFilePath.size() == 0 || pcmFilePath.size() == 0) {
        INFO("error in wav2pcm");
        return false;
    }
    std::string cmd =
        "ffmpeg -i " + wavFilePath + " -f s16le -ar 8000 -ac 1 -acodec pcm_s16le -y " + pcmFilePath;
    int ret = system(cmd.c_str());
    INFO("{}", ret);
    return true;
}



}  // namespace cill