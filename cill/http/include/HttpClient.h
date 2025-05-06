#pragma once

#include <curl/curl.h>

#include <map>
#include <string>

namespace cill {


class HttpClient {
    public:
        HttpClient() {}
        HttpClient(const std::string& webApi);
        void reset(const std::string& webApi);
        bool postJson(std::string& response, const std::string& body,
                    const std::map<std::string, std::string>& head);
        bool upload(std::string& outputText, const std::string& sid, const std::string& fileName,
                    const std::string& filePath);
        std::string getWebApi();

    private:
        bool postRequest(std::string& response, CURL* curl);

    private:
        std::string webApi_;
        std::string host_;
};


}  // namespace cill