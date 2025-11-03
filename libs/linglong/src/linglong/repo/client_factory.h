/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

extern "C" {
#include "api/ClientAPI.h"
}

#include "configure.h"

#include <QEventLoop>

#include <memory>
#include <thread>

#define SYNCREQ(type, func, ...) \
    syncRun<type##_t, decltype(&type##_free)>(func, type##_free, __VA_ARGS__)

namespace linglong::repo {

class ClientAPIWrapper
{
public:
    explicit ClientAPIWrapper(apiClient_t *client)
        : client(client)
    {
        client->userAgent = m_user_agent.c_str();
    }

    virtual ~ClientAPIWrapper() { apiClient_free(client); }

    template <typename R, typename D, typename T, typename... Args>
    auto syncRun(T func, D deleter, Args... args) -> std::unique_ptr<R, D>
    {
        QEventLoop loop;
        R *response = nullptr;
        std::thread t(
          [&loop, &func, &response](apiClient_t *client, Args... args) {
              response = func(client, args...);
              loop.exit();
          },
          client,
          args...);
        loop.exec();
        if (t.joinable()) {
            t.join();
        }
        return std::unique_ptr<R, D>(response, deleter);
    }

    virtual auto fuzzySearch(request_fuzzy_search_req_t *req)
      -> std::unique_ptr<fuzzy_search_app_200_response_t,
                         decltype(&fuzzy_search_app_200_response_free)>
    {
        return SYNCREQ(fuzzy_search_app_200_response, ClientAPI_fuzzySearchApp, req);
    }

    virtual auto signIn(request_auth_t *req)
      -> std::unique_ptr<sign_in_200_response_t, decltype(&sign_in_200_response_free)>
    {
        return SYNCREQ(sign_in_200_response, ClientAPI_signIn, req);
    }

    virtual auto newUploadTaskID(char *token, schema_new_upload_task_req_t *req)
      -> std::unique_ptr<new_upload_task_id_200_response_t,
                         decltype(&new_upload_task_id_200_response_free)>
    {
        return SYNCREQ(new_upload_task_id_200_response, ClientAPI_newUploadTaskID, token, req);
    }

    virtual auto uploadTaskFile(char *token, char *taskID, binary_t *binary)
      -> std::unique_ptr<api_upload_task_file_resp_t, decltype(&api_upload_task_file_resp_free)>
    {
        return SYNCREQ(api_upload_task_file_resp, ClientAPI_uploadTaskFile, token, taskID, binary);
    }

    virtual auto uploadTaskInfo(char *token, char *taskID)
      -> std::unique_ptr<upload_task_info_200_response_t,
                         decltype(&upload_task_info_200_response_free)>
    {
        return SYNCREQ(upload_task_info_200_response, ClientAPI_uploadTaskInfo, token, taskID);
    }

private:
    apiClient_t *client;
    std::string m_user_agent = "linglong/" LINGLONG_VERSION;
};

class ClientFactory
{
public:
    explicit ClientFactory(std::string server);

    std::unique_ptr<ClientAPIWrapper> createClientV2();

private:
    std::string m_server;
};

} // namespace linglong::repo
