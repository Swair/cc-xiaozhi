#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "AlsaAudio.h"
#include "HttpClient.h"
#include "Json.h"
#include "Log.h"
#include "Opus.h"
#include "Websocket.h"

using namespace cill;

// 全局配置
const std::string ota_url = "https://api.tenclass.net/xiaozhi/ota/";
const std::string ws_url = "wss://api.tenclass.net/xiaozhi/v1/";
const std::string access_token = "test-token";
const std::string device_mac = "30:ed:a0:30:cd:b4";
const std::string device_uuid = "test-uuid";

int CHUNK = 960;
int SAMPLE_RATE = 16000;
int CHANNELS = 1;

// 全局状态
struct AudioState {
    std::atomic<bool> running{true};
    std::string listen_state = "stop";
    std::string tts_state = "idle";
    std::string session_id;
};

AlsaAudio audio;
OpusAudio opus(SAMPLE_RATE, CHANNELS);
AudioState xiaozhiai_state;
WebSocketClient ws_client(ws_url);

// 发送硬件信息，获取固件版本
void get_ota_version() {
    json ota_post_data = {
        {"flash_size", 16777216},
        {"minimum_free_heap_size", 8318916},
        {"mac_address", device_mac},
        {"chip_model_name", "esp32s3"},
        {"chip_info", {{"model", 9}, {"cores", 2}, {"revision", 2}, {"features", 18}}},
        {"application", {{"name", "xiaozhi"}, {"version", "1.6.0"}}},
        {"partition_table", json::array()},
        {"ota", {{"label", "factory"}}},
        {"board", {{"type", "bread-compact-wifi"}, {"ip", "192.168.124.38"}, {"mac", device_mac}}}};

    std::string post_data = ota_post_data.dump();
    std::string response;
    cill::HttpClient hc;
    hc.reset(ota_url);
    std::map<std::string, std::string> header;
    header["Device-Id"] = device_mac;
    hc.postJson(response, post_data, header);

    INFO("OTA Request:{}", post_data);
    INFO("OTA Response:{}", response);
}

int main() {
    try {
        // 获取OTA信息
        get_ota_version();

        // 启动音频线程
        std::thread audio_thread = std::thread([]() {
            audio.Init();
            std::vector<unsigned char> opus_buffer(CHUNK);
            short buffer[CHUNK] = {};

            while (xiaozhiai_state.running) {
                audio.Read(buffer, CHUNK);

                if (xiaozhiai_state.listen_state != "start") {
                    usleep(10 * 1000);
                    continue;
                }

                opus_buffer.insert(opus_buffer.end(), buffer, buffer + CHUNK);
                int encoded = opus.Encode(opus_buffer.data(), 2 * CHUNK, buffer, CHUNK);
                if (encoded > 0) {
                    ws_client.send_binary(opus_buffer.data(), encoded);
                }
                opus_buffer.clear();
                usleep(10 * 1000);
            }
        });

        // 启动websocket线程
        std::thread ws_thread = std::thread([]() {
            std::map<std::string, std::string> headers;
            headers["Authorization"] = "Bearer " + access_token;
            headers["Protocol-Version"] = "1";
            headers["Device-Id"] = device_mac;
            headers["Client-Id"] = device_uuid;

            ws_client.SetWsHeaders(headers);
            ws_client.SetOnOpenCallback([&]() -> std::string {
                INFO("on open");
                json hello_msg = {{"type", "hello"},
                                  {"version", 1},
                                  {"transport", "websocket"},
                                  {"audio_params",
                                   {{"format", "opus"},
                                    {"sample_rate", SAMPLE_RATE},
                                    {"channels", CHANNELS},
                                    {"frame_duration", 60}}}};
                return hello_msg.dump();
            });

            ws_client.SetOnCloseCallback([]() {
                xiaozhiai_state.listen_state = "stop";
                xiaozhiai_state.running = false;
                INFO("WebSocket disconnected");
            });

            ws_client.SetOnFailCallback([]() { ERROR("WebSocket connection failed"); });

            ws_client.SetOnMessageCallback([](const std::string& msg, bool binary) -> std::string {
                if (binary) {
                    // INFO("<< binary data");
                    // 处理二进制音频数据
                    std::vector<opus_int16> pcm_data(CHUNK);
                    int decoded = opus.Decode(pcm_data.data(), CHUNK, (unsigned char*)(msg.data()),
                                              msg.size());
                    if (decoded > 0) {
                        short buffer[CHUNK] = {};
                        memcpy((char*)&buffer[0], (char*)&pcm_data[0], CHUNK * 2);
                        audio.Write(buffer, CHUNK);
                    }
                    return "";
                } else {
                    // 处理文本消息
                    try {
                        std::string resmsg = msg;
                        INFO("<< {}", resmsg);
                        json received_msg = json::parse(resmsg);
                        if (received_msg["type"] == "hello") {
                            xiaozhiai_state.session_id = received_msg["session_id"];
                            json start_msg = {{"session_id", xiaozhiai_state.session_id},
                                              {"type", "listen"},
                                              {"state", "start"},
                                              {"mode", "auto"}};

                            xiaozhiai_state.listen_state = "start";
                            INFO("");
                            return start_msg.dump();
                        }

                        if (received_msg["type"] == "tts") {
                            xiaozhiai_state.tts_state = received_msg["state"];
                        }

                        if (xiaozhiai_state.tts_state == "stop") {
                            xiaozhiai_state.session_id = received_msg["session_id"];
                            json start_msg = {{"session_id", xiaozhiai_state.session_id},
                                              {"type", "listen"},
                                              {"state", "start"},
                                              {"mode", "auto"}};

                            xiaozhiai_state.listen_state = "start";
                            INFO("");
                            return start_msg.dump();
                        }

                        if (xiaozhiai_state.tts_state == "start") {
                            xiaozhiai_state.listen_state = "stop";
                        }

                        if (received_msg["type"] == "goodbye" &&
                            xiaozhiai_state.session_id == received_msg["session_id"]) {
                            INFO("<< Goodbye");
                            xiaozhiai_state.session_id = "";
                        }

                    } catch (const std::exception& e) {
                        std::cerr << "Message parse error: " << e.what() << std::endl;
                    }
                }
                return "";
            });

            ws_client.start();
        });

        // 主线程等待
        INFO("Press Enter to exit...");
        std::cin.get();
        xiaozhiai_state.running = false;
        if (audio_thread.joinable()) {
            audio_thread.join();
        }
        if (ws_thread.joinable()) {
            ws_thread.join();
        }

    } catch (const std::exception& e) {
        ERROR("Fatal error: {}", e.what());
        return -1;
    }

    return 0;
}