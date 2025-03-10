#include <stdlib.h>
#include <stdio.h>
#include "../include/apiClient.h"
#include "../include/list.h"
#include "../external/cJSON.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "../model/api_upload_task_file_resp.h"
#include "../model/api_upload_task_layer_file_resp.h"
#include "../model/fuzzy_search_app_200_response.h"
#include "../model/get_repo_200_response.h"
#include "../model/new_upload_task_id_200_response.h"
#include "../model/request_auth.h"
#include "../model/request_fuzzy_search_req.h"
#include "../model/schema_new_upload_task_req.h"
#include "../model/sign_in_200_response.h"
#include "../model/upload_task_info_200_response.h"


// 模糊查找App
//
fuzzy_search_app_200_response_t*
ClientAPI_fuzzySearchApp(apiClient_t *apiClient, request_fuzzy_search_req_t *data);


// 查看仓库信息
//
// returns repository mode and resolve all branches
//
get_repo_200_response_t*
ClientAPI_getRepo(apiClient_t *apiClient, char *repo);


// generate a new upload task id
//
// generate a new upload task id
//
new_upload_task_id_200_response_t*
ClientAPI_newUploadTaskID(apiClient_t *apiClient, char *X_Token, schema_new_upload_task_req_t *req);


// 登陆帐号
//
sign_in_200_response_t*
ClientAPI_signIn(apiClient_t *apiClient, request_auth_t *data);


// upload tgz file to upload task
//
// upload tgz file to upload task
//
api_upload_task_file_resp_t*
ClientAPI_uploadTaskFile(apiClient_t *apiClient, char *X_Token, char *task_id, binary_t* file);


// get upload task status
//
// get upload task status
//
upload_task_info_200_response_t*
ClientAPI_uploadTaskInfo(apiClient_t *apiClient, char *X_Token, char *task_id);


// upload layer file to upload task
//
api_upload_task_layer_file_resp_t*
ClientAPI_uploadTaskLayerFile(apiClient_t *apiClient, char *X_Token, char *task_id, binary_t* file);


