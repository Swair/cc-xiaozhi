#include "HttpClient.h"

#include "Json.h"
#include "Log.h"

int getContent(std::string& content, const std::string& inputText, const std::string& bsig,
               int offset, const std::string& esig) {
    auto bi = inputText.find(bsig);
    if (bi == std::string::npos) return 0;

    auto bText = inputText.substr(bi + bsig.size() + offset);

    auto ei = bText.find(esig);
    if (ei == std::string::npos) return 0;

    content = bText.substr(0, ei);

    return 1;
}

namespace cill {

static size_t write_data(void* buffer, size_t size, size_t nmemb, void* stream) {
    std::string* pStream = static_cast<std::string*>(stream);
    (*pStream).append((char*)buffer, size * nmemb);
    return size * nmemb;
}

HttpClient::HttpClient(const std::string& webApi) {
    webApi_ = webApi;
    getContent(host_, webApi_, std::string("//"), 0, std::string("/"));
}

void HttpClient::reset(const std::string& webApi) {
    webApi_ = webApi;
    getContent(host_, webApi_, std::string("//"), 0, std::string("/"));
}

std::string HttpClient::getWebApi() { return webApi_; }

bool HttpClient::postRequest(std::string& response, CURL* curl) {
    INFO("{}", webApi_.c_str());
    std::string* pStream = &response;
    curl_easy_setopt(curl, CURLOPT_URL, (char*)webApi_.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, pStream);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::string errorMsg;
        switch (res) {
            case CURLE_UNSUPPORTED_PROTOCOL:
                errorMsg = "postRequest, algWarning, HttpClient: curl不支持的协议,由URL的头部指定";
                break;
            case CURLE_COULDNT_CONNECT:
                errorMsg = "postRequest, algWarning, HttpClient, curl不能连接到remote主机或者代理";
                break;
            case CURLE_HTTP_RETURNED_ERROR:
                errorMsg = "postRequest, algWarning, HttpClient, curl http返回错误";
                break;
            case CURLE_READ_ERROR:
                errorMsg = "postRequest, algWarning, HttpClient: curl读本地文件错误";
                break;
            default:
                errorMsg = "postRequest, algWarning, HttpClient, curl提交出错, 返回值: " +
                           std::to_string(res);
        }

        curl_easy_cleanup(curl);
        ERROR("{}", errorMsg);
        return false;
    }
    curl_easy_cleanup(curl);
    return true;
}

bool HttpClient::postJson(std::string& response, const std::string& body,
                          const std::map<std::string, std::string>& head) {
    bool ret = false;
    response.clear();
    std::string headValue;
    if (body.empty()) {
        ERROR("HttpClient::postJson, the body is null");
        ret = false;
    } else {
        // std::string hostHead = "Host:" + host_;
        std::string contentLengthHead = "Content-Length:" + std::to_string(body.size());

        CURL* curl = curl_easy_init();
        if (curl) {
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Accept:application/json");
            headers = curl_slist_append(headers, "Content-Type:application/json");
            // headers = curl_slist_append(headers, "charset:utf-8");
            // headers = curl_slist_append(headers, "User-Agent:NumX");
            // headers = curl_slist_append(headers, (char*)hostHead.c_str());
            // headers = curl_slist_append(headers, (char*)contentLengthHead.c_str());
            for (auto& item : head) {
                headValue = item.first + ":" + item.second;
                headers = curl_slist_append(headers, (char*)headValue.c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char*)body.c_str());
            ret = postRequest(response, curl);
            // INFO("HttpClient::postJson, {}", response);
        } else {
            ERROR("postJson, curl failed");
            return false;
        }

        try {
            json jOut = json::parse(response);  // 判定返回值是否是json
        } catch (nlohmann::detail::exception& e) {
            if (response.empty()) {
                ERROR("postJson, the result is not json, is null");
            } else {
                ERROR("postJson, the result is not json, is {}", response);
            }
            return false;
        }
    }
    return ret;
}

bool HttpClient::upload(std::string& outputText, const std::string& sid,
                        const std::string& fileName, const std::string& filePath) {
    CURL* curl = curl_easy_init();
    if (curl) {
        struct curl_httppost* post = NULL;
        struct curl_httppost* last = NULL;

#if 0
        for(auto& i : kv)
        {
            curl_formadd(&post, &last, CURLFORM_PTRNAME, i.first.c_str(), CURLFORM_PTRCONTENTS, i.second.c_str(), CURLFORM_END);
        }
#else
        curl_formadd(&post, &last, CURLFORM_PTRNAME, "sid", CURLFORM_PTRCONTENTS, sid.c_str(),
                     CURLFORM_END);
        // curl_formadd(&post, &last, CURLFORM_PTRNAME, "appCode", CURLFORM_PTRCONTENTS,
        // appCode.c_str(), CURLFORM_END);
#endif
        curl_formadd(&post, &last, CURLFORM_PTRNAME, "fileType", CURLFORM_PTRCONTENTS, "mp3",
                     CURLFORM_END);
        curl_formadd(&post, &last, CURLFORM_PTRNAME, "file", CURLFORM_FILE, filePath.c_str(),
                     CURLFORM_FILENAME, fileName.c_str(), CURLFORM_END);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);

        return postRequest(outputText, curl);
    } else {
        ERROR("postJson, curl failed");
        return false;
    }
}
}  // namespace cill