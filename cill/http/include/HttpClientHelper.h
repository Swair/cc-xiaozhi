#pragma once

#include "Common.h"
#include "HttpClient.h"

namespace cill {
class HttpClientHelper {
    public:
        void post(std::string& response, const std::string& request) {
            if (request.size() == 0) {
                throw Exception("RemoteCall, post, the request size is 0");
            }
            std::map<std::string, std::string> head;
            INFO("RemoteCall " + request);
            hc_.postJson(response, request, head);
        }
        void uploadFile(std::string& outputText, const std::string& sid, const std::string& filePath) {
            int index = filePath.rfind("/");
            std::string fileName = filePath.substr(0, index);
            INFO("fileName, " + fileName + ", filePath, " + filePath);
            hc_.upload(outputText, sid, fileName, filePath);
        }

        void reset(const std::string& webApi) { hc_.reset(webApi); }

    public:
        HttpClient hc_;
};
}  // namespace cill