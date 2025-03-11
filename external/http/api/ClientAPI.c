#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "ClientAPI.h"

#define MAX_NUMBER_LENGTH 16
#define MAX_BUFFER_LENGTH 4096
#define intToStr(dst, src) \
    do {\
    char dst[256];\
    snprintf(dst, 256, "%ld", (long int)(src));\
}while(0)


// 查看仓库列表
//
// returns repository mode and resolve all branches
//
_api_v1_repos_get_200_response_t*
ClientAPI_apiV1ReposGet(apiClient_t *apiClient)
{
    list_t    *localVarQueryParameters = NULL;
    list_t    *localVarHeaderParameters = NULL;
    list_t    *localVarFormParameters = NULL;
    list_t *localVarHeaderType = list_createList();
    list_t *localVarContentType = NULL;
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v1/repos")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v1/repos");



    list_addElement(localVarHeaderType,"*/*"); //produces
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "GET");

    // uncomment below to debug the error response
    //if (apiClient->response_code == 200) {
    //    printf("%s\n","OK");
    //}
    //nonprimitive not container
    cJSON *ClientAPIlocalVarJSON = cJSON_Parse(apiClient->dataReceived);
    _api_v1_repos_get_200_response_t *elementToReturn = _api_v1_repos_get_200_response_parseFromJSON(ClientAPIlocalVarJSON);
    cJSON_Delete(ClientAPIlocalVarJSON);
    if(elementToReturn == NULL) {
        // return 0;
    }

    //return type
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    
    
    
    list_freeList(localVarHeaderType);
    
    free(localVarPath);
    return elementToReturn;
end:
    free(localVarPath);
    return NULL;

}

// 模糊查找App
//
fuzzy_search_app_200_response_t*
ClientAPI_fuzzySearchApp(apiClient_t *apiClient, request_fuzzy_search_req_t *data)
{
    list_t    *localVarQueryParameters = NULL;
    list_t    *localVarHeaderParameters = NULL;
    list_t    *localVarFormParameters = NULL;
    list_t *localVarHeaderType = list_createList();
    list_t *localVarContentType = list_createList();
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v0/apps/fuzzysearchapp")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v0/apps/fuzzysearchapp");




    // Body Param
    cJSON *localVarSingleItemJSON_data = NULL;
    if (data != NULL)
    {
        //string
        localVarSingleItemJSON_data = request_fuzzy_search_req_convertToJSON(data);
        localVarBodyParameters = cJSON_Print(localVarSingleItemJSON_data);
    }
    list_addElement(localVarHeaderType,"application/json"); //produces
    list_addElement(localVarContentType,"application/json"); //consumes
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "POST");

    // uncomment below to debug the error response
    //if (apiClient->response_code == 200) {
    //    printf("%s\n","OK");
    //}
    //nonprimitive not container
    cJSON *ClientAPIlocalVarJSON = cJSON_Parse(apiClient->dataReceived);
    fuzzy_search_app_200_response_t *elementToReturn = fuzzy_search_app_200_response_parseFromJSON(ClientAPIlocalVarJSON);
    cJSON_Delete(ClientAPIlocalVarJSON);
    if(elementToReturn == NULL) {
        // return 0;
    }

    //return type
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    
    
    
    list_freeList(localVarHeaderType);
    list_freeList(localVarContentType);
    free(localVarPath);
    if (localVarSingleItemJSON_data) {
        cJSON_Delete(localVarSingleItemJSON_data);
        localVarSingleItemJSON_data = NULL;
    }
    free(localVarBodyParameters);
    return elementToReturn;
end:
    free(localVarPath);
    return NULL;

}

// 查看仓库信息
//
// returns repository mode and resolve all branches
//
get_repo_200_response_t*
ClientAPI_getRepo(apiClient_t *apiClient, char *repo)
{
    list_t    *localVarQueryParameters = NULL;
    list_t    *localVarHeaderParameters = NULL;
    list_t    *localVarFormParameters = NULL;
    list_t *localVarHeaderType = list_createList();
    list_t *localVarContentType = NULL;
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v1/repos/{repo}")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v1/repos/{repo}");


    // Path Params
    long sizeOfPathParams_repo = strlen(repo)+3 + strlen("{ repo }");
    if(repo == NULL) {
        goto end;
    }
    char* localVarToReplace_repo = malloc(sizeOfPathParams_repo);
    sprintf(localVarToReplace_repo, "{%s}", "repo");

    localVarPath = strReplace(localVarPath, localVarToReplace_repo, repo);


    list_addElement(localVarHeaderType,"*/*"); //produces
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "GET");

    // uncomment below to debug the error response
    //if (apiClient->response_code == 200) {
    //    printf("%s\n","OK");
    //}
    //nonprimitive not container
    cJSON *ClientAPIlocalVarJSON = cJSON_Parse(apiClient->dataReceived);
    get_repo_200_response_t *elementToReturn = get_repo_200_response_parseFromJSON(ClientAPIlocalVarJSON);
    cJSON_Delete(ClientAPIlocalVarJSON);
    if(elementToReturn == NULL) {
        // return 0;
    }

    //return type
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    
    
    
    list_freeList(localVarHeaderType);
    
    free(localVarPath);
    free(localVarToReplace_repo);
    return elementToReturn;
end:
    free(localVarPath);
    return NULL;

}

// generate a new upload task id
//
// generate a new upload task id
//
new_upload_task_id_200_response_t*
ClientAPI_newUploadTaskID(apiClient_t *apiClient, char *X_Token, schema_new_upload_task_req_t *req)
{
    list_t    *localVarQueryParameters = NULL;
    list_t    *localVarHeaderParameters = list_createList();
    list_t    *localVarFormParameters = NULL;
    list_t *localVarHeaderType = list_createList();
    list_t *localVarContentType = NULL;
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v1/upload-tasks")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v1/upload-tasks");




    // header parameters
    char *keyHeader_X_Token = NULL;
    char * valueHeader_X_Token = 0;
    keyValuePair_t *keyPairHeader_X_Token = 0;
    if (X_Token) {
        keyHeader_X_Token = strdup("X-Token");
        valueHeader_X_Token = strdup((X_Token));
        keyPairHeader_X_Token = keyValuePair_create(keyHeader_X_Token, valueHeader_X_Token);
        list_addElement(localVarHeaderParameters,keyPairHeader_X_Token);
    }


    // Body Param
    cJSON *localVarSingleItemJSON_req = NULL;
    if (req != NULL)
    {
        //string
        localVarSingleItemJSON_req = schema_new_upload_task_req_convertToJSON(req);
        localVarBodyParameters = cJSON_Print(localVarSingleItemJSON_req);
    }
    list_addElement(localVarHeaderType,"*/*"); //produces
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "POST");

    // uncomment below to debug the error response
    //if (apiClient->response_code == 200) {
    //    printf("%s\n","OK");
    //}
    //nonprimitive not container
    cJSON *ClientAPIlocalVarJSON = cJSON_Parse(apiClient->dataReceived);
    new_upload_task_id_200_response_t *elementToReturn = new_upload_task_id_200_response_parseFromJSON(ClientAPIlocalVarJSON);
    cJSON_Delete(ClientAPIlocalVarJSON);
    if(elementToReturn == NULL) {
        // return 0;
    }

    //return type
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    
    list_freeList(localVarHeaderParameters);
    
    list_freeList(localVarHeaderType);
    
    free(localVarPath);
    if (keyHeader_X_Token) {
        free(keyHeader_X_Token);
        keyHeader_X_Token = NULL;
    }
    if (valueHeader_X_Token) {
        free(valueHeader_X_Token);
        valueHeader_X_Token = NULL;
    }
    free(keyPairHeader_X_Token);
    if (localVarSingleItemJSON_req) {
        cJSON_Delete(localVarSingleItemJSON_req);
        localVarSingleItemJSON_req = NULL;
    }
    free(localVarBodyParameters);
    return elementToReturn;
end:
    free(localVarPath);
    return NULL;

}

// delete a ref from repo
//
// delete a ref from repo
//
void
ClientAPI_refDelete(apiClient_t *apiClient, char *X_Token, char *repo, char *channel, char *app_id, char *version, char *arch, char *module, char *hard)
{
    list_t    *localVarQueryParameters = list_createList();
    list_t    *localVarHeaderParameters = list_createList();
    list_t    *localVarFormParameters = NULL;
    list_t *localVarHeaderType = NULL;
    list_t *localVarContentType = NULL;
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v1/repos/{repo}/refs/{channel}/{app_id}/{version}/{arch}/{module}")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v1/repos/{repo}/refs/{channel}/{app_id}/{version}/{arch}/{module}");


    // Path Params
    long sizeOfPathParams_repo = strlen(repo)+3 + strlen(channel)+3 + strlen(app_id)+3 + strlen(version)+3 + strlen(arch)+3 + strlen(module)+3 + strlen("{ repo }");
    if(repo == NULL) {
        goto end;
    }
    char* localVarToReplace_repo = malloc(sizeOfPathParams_repo);
    sprintf(localVarToReplace_repo, "{%s}", "repo");

    localVarPath = strReplace(localVarPath, localVarToReplace_repo, repo);

    // Path Params
    long sizeOfPathParams_channel = strlen(repo)+3 + strlen(channel)+3 + strlen(app_id)+3 + strlen(version)+3 + strlen(arch)+3 + strlen(module)+3 + strlen("{ channel }");
    if(channel == NULL) {
        goto end;
    }
    char* localVarToReplace_channel = malloc(sizeOfPathParams_channel);
    sprintf(localVarToReplace_channel, "{%s}", "channel");

    localVarPath = strReplace(localVarPath, localVarToReplace_channel, channel);

    // Path Params
    long sizeOfPathParams_app_id = strlen(repo)+3 + strlen(channel)+3 + strlen(app_id)+3 + strlen(version)+3 + strlen(arch)+3 + strlen(module)+3 + strlen("{ app_id }");
    if(app_id == NULL) {
        goto end;
    }
    char* localVarToReplace_app_id = malloc(sizeOfPathParams_app_id);
    sprintf(localVarToReplace_app_id, "{%s}", "app_id");

    localVarPath = strReplace(localVarPath, localVarToReplace_app_id, app_id);

    // Path Params
    long sizeOfPathParams_version = strlen(repo)+3 + strlen(channel)+3 + strlen(app_id)+3 + strlen(version)+3 + strlen(arch)+3 + strlen(module)+3 + strlen("{ version }");
    if(version == NULL) {
        goto end;
    }
    char* localVarToReplace_version = malloc(sizeOfPathParams_version);
    sprintf(localVarToReplace_version, "{%s}", "version");

    localVarPath = strReplace(localVarPath, localVarToReplace_version, version);

    // Path Params
    long sizeOfPathParams_arch = strlen(repo)+3 + strlen(channel)+3 + strlen(app_id)+3 + strlen(version)+3 + strlen(arch)+3 + strlen(module)+3 + strlen("{ arch }");
    if(arch == NULL) {
        goto end;
    }
    char* localVarToReplace_arch = malloc(sizeOfPathParams_arch);
    sprintf(localVarToReplace_arch, "{%s}", "arch");

    localVarPath = strReplace(localVarPath, localVarToReplace_arch, arch);

    // Path Params
    long sizeOfPathParams_module = strlen(repo)+3 + strlen(channel)+3 + strlen(app_id)+3 + strlen(version)+3 + strlen(arch)+3 + strlen(module)+3 + strlen("{ module }");
    if(module == NULL) {
        goto end;
    }
    char* localVarToReplace_module = malloc(sizeOfPathParams_module);
    sprintf(localVarToReplace_module, "{%s}", "module");

    localVarPath = strReplace(localVarPath, localVarToReplace_module, module);



    // header parameters
    char *keyHeader_X_Token = NULL;
    char * valueHeader_X_Token = 0;
    keyValuePair_t *keyPairHeader_X_Token = 0;
    if (X_Token) {
        keyHeader_X_Token = strdup("X-Token");
        valueHeader_X_Token = strdup((X_Token));
        keyPairHeader_X_Token = keyValuePair_create(keyHeader_X_Token, valueHeader_X_Token);
        list_addElement(localVarHeaderParameters,keyPairHeader_X_Token);
    }


    // query parameters
    char *keyQuery_hard = NULL;
    char * valueQuery_hard = NULL;
    keyValuePair_t *keyPairQuery_hard = 0;
    if (hard)
    {
        keyQuery_hard = strdup("hard");
        valueQuery_hard = strdup((hard));
        keyPairQuery_hard = keyValuePair_create(keyQuery_hard, valueQuery_hard);
        list_addElement(localVarQueryParameters,keyPairQuery_hard);
    }
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "DELETE");

    //No return type
end:
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    list_freeList(localVarQueryParameters);
    list_freeList(localVarHeaderParameters);
    
    
    
    free(localVarPath);
    free(localVarToReplace_repo);
    free(localVarToReplace_channel);
    free(localVarToReplace_app_id);
    free(localVarToReplace_version);
    free(localVarToReplace_arch);
    free(localVarToReplace_module);
    if (keyHeader_X_Token) {
        free(keyHeader_X_Token);
        keyHeader_X_Token = NULL;
    }
    if (valueHeader_X_Token) {
        free(valueHeader_X_Token);
        valueHeader_X_Token = NULL;
    }
    free(keyPairHeader_X_Token);
    if(keyQuery_hard){
        free(keyQuery_hard);
        keyQuery_hard = NULL;
    }
    if(valueQuery_hard){
        free(valueQuery_hard);
        valueQuery_hard = NULL;
    }
    if(keyPairQuery_hard){
        keyValuePair_free(keyPairQuery_hard);
        keyPairQuery_hard = NULL;
    }
    if(keyQuery_hard){
        free(keyQuery_hard);
        keyQuery_hard = NULL;
    }
    if(keyPairQuery_hard){
        keyValuePair_free(keyPairQuery_hard);
        keyPairQuery_hard = NULL;
    }

}

// 查找App
//
apiv2_search_app_response_t*
ClientAPI_searchApp(apiClient_t *apiClient, char *repo_name, char *channel, char *app_id, char *arch, char *module, char *version)
{
    list_t    *localVarQueryParameters = list_createList();
    list_t    *localVarHeaderParameters = NULL;
    list_t    *localVarFormParameters = NULL;
    list_t *localVarHeaderType = list_createList();
    list_t *localVarContentType = NULL;
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v2/search/apps")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v2/search/apps");




    // query parameters
    char *keyQuery_repo_name = NULL;
    char * valueQuery_repo_name = NULL;
    keyValuePair_t *keyPairQuery_repo_name = 0;
    if (repo_name)
    {
        keyQuery_repo_name = strdup("repo_name");
        valueQuery_repo_name = strdup((repo_name));
        keyPairQuery_repo_name = keyValuePair_create(keyQuery_repo_name, valueQuery_repo_name);
        list_addElement(localVarQueryParameters,keyPairQuery_repo_name);
    }

    // query parameters
    char *keyQuery_channel = NULL;
    char * valueQuery_channel = NULL;
    keyValuePair_t *keyPairQuery_channel = 0;
    if (channel)
    {
        keyQuery_channel = strdup("channel");
        valueQuery_channel = strdup((channel));
        keyPairQuery_channel = keyValuePair_create(keyQuery_channel, valueQuery_channel);
        list_addElement(localVarQueryParameters,keyPairQuery_channel);
    }

    // query parameters
    char *keyQuery_app_id = NULL;
    char * valueQuery_app_id = NULL;
    keyValuePair_t *keyPairQuery_app_id = 0;
    if (app_id)
    {
        keyQuery_app_id = strdup("app_id");
        valueQuery_app_id = strdup((app_id));
        keyPairQuery_app_id = keyValuePair_create(keyQuery_app_id, valueQuery_app_id);
        list_addElement(localVarQueryParameters,keyPairQuery_app_id);
    }

    // query parameters
    char *keyQuery_arch = NULL;
    char * valueQuery_arch = NULL;
    keyValuePair_t *keyPairQuery_arch = 0;
    if (arch)
    {
        keyQuery_arch = strdup("arch");
        valueQuery_arch = strdup((arch));
        keyPairQuery_arch = keyValuePair_create(keyQuery_arch, valueQuery_arch);
        list_addElement(localVarQueryParameters,keyPairQuery_arch);
    }

    // query parameters
    char *keyQuery_module = NULL;
    char * valueQuery_module = NULL;
    keyValuePair_t *keyPairQuery_module = 0;
    if (module)
    {
        keyQuery_module = strdup("module");
        valueQuery_module = strdup((module));
        keyPairQuery_module = keyValuePair_create(keyQuery_module, valueQuery_module);
        list_addElement(localVarQueryParameters,keyPairQuery_module);
    }

    // query parameters
    char *keyQuery_version = NULL;
    char * valueQuery_version = NULL;
    keyValuePair_t *keyPairQuery_version = 0;
    if (version)
    {
        keyQuery_version = strdup("version");
        valueQuery_version = strdup((version));
        keyPairQuery_version = keyValuePair_create(keyQuery_version, valueQuery_version);
        list_addElement(localVarQueryParameters,keyPairQuery_version);
    }
    list_addElement(localVarHeaderType,"application/json"); //produces
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "GET");

    // uncomment below to debug the error response
    //if (apiClient->response_code == 200) {
    //    printf("%s\n","OK");
    //}
    // uncomment below to debug the error response
    //if (apiClient->response_code == 400) {
    //    printf("%s\n","Bad Request");
    //}
    // uncomment below to debug the error response
    //if (apiClient->response_code == 500) {
    //    printf("%s\n","Internal Server Error");
    //}
    //nonprimitive not container
    cJSON *ClientAPIlocalVarJSON = cJSON_Parse(apiClient->dataReceived);
    apiv2_search_app_response_t *elementToReturn = apiv2_search_app_response_parseFromJSON(ClientAPIlocalVarJSON);
    cJSON_Delete(ClientAPIlocalVarJSON);
    if(elementToReturn == NULL) {
        // return 0;
    }

    //return type
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    list_freeList(localVarQueryParameters);
    
    
    list_freeList(localVarHeaderType);
    
    free(localVarPath);
    if(keyQuery_repo_name){
        free(keyQuery_repo_name);
        keyQuery_repo_name = NULL;
    }
    if(valueQuery_repo_name){
        free(valueQuery_repo_name);
        valueQuery_repo_name = NULL;
    }
    if(keyPairQuery_repo_name){
        keyValuePair_free(keyPairQuery_repo_name);
        keyPairQuery_repo_name = NULL;
    }
    if(keyQuery_channel){
        free(keyQuery_channel);
        keyQuery_channel = NULL;
    }
    if(valueQuery_channel){
        free(valueQuery_channel);
        valueQuery_channel = NULL;
    }
    if(keyPairQuery_channel){
        keyValuePair_free(keyPairQuery_channel);
        keyPairQuery_channel = NULL;
    }
    if(keyQuery_app_id){
        free(keyQuery_app_id);
        keyQuery_app_id = NULL;
    }
    if(valueQuery_app_id){
        free(valueQuery_app_id);
        valueQuery_app_id = NULL;
    }
    if(keyPairQuery_app_id){
        keyValuePair_free(keyPairQuery_app_id);
        keyPairQuery_app_id = NULL;
    }
    if(keyQuery_arch){
        free(keyQuery_arch);
        keyQuery_arch = NULL;
    }
    if(valueQuery_arch){
        free(valueQuery_arch);
        valueQuery_arch = NULL;
    }
    if(keyPairQuery_arch){
        keyValuePair_free(keyPairQuery_arch);
        keyPairQuery_arch = NULL;
    }
    if(keyQuery_module){
        free(keyQuery_module);
        keyQuery_module = NULL;
    }
    if(valueQuery_module){
        free(valueQuery_module);
        valueQuery_module = NULL;
    }
    if(keyPairQuery_module){
        keyValuePair_free(keyPairQuery_module);
        keyPairQuery_module = NULL;
    }
    if(keyQuery_version){
        free(keyQuery_version);
        keyQuery_version = NULL;
    }
    if(valueQuery_version){
        free(valueQuery_version);
        valueQuery_version = NULL;
    }
    if(keyPairQuery_version){
        keyValuePair_free(keyPairQuery_version);
        keyPairQuery_version = NULL;
    }
    return elementToReturn;
end:
    free(localVarPath);
    return NULL;

}

// 登陆帐号
//
sign_in_200_response_t*
ClientAPI_signIn(apiClient_t *apiClient, request_auth_t *data)
{
    list_t    *localVarQueryParameters = NULL;
    list_t    *localVarHeaderParameters = NULL;
    list_t    *localVarFormParameters = NULL;
    list_t *localVarHeaderType = list_createList();
    list_t *localVarContentType = NULL;
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v1/sign-in")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v1/sign-in");




    // Body Param
    cJSON *localVarSingleItemJSON_data = NULL;
    if (data != NULL)
    {
        //string
        localVarSingleItemJSON_data = request_auth_convertToJSON(data);
        localVarBodyParameters = cJSON_Print(localVarSingleItemJSON_data);
    }
    list_addElement(localVarHeaderType,"*/*"); //produces
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "POST");

    // uncomment below to debug the error response
    //if (apiClient->response_code == 200) {
    //    printf("%s\n","OK");
    //}
    //nonprimitive not container
    cJSON *ClientAPIlocalVarJSON = cJSON_Parse(apiClient->dataReceived);
    sign_in_200_response_t *elementToReturn = sign_in_200_response_parseFromJSON(ClientAPIlocalVarJSON);
    cJSON_Delete(ClientAPIlocalVarJSON);
    if(elementToReturn == NULL) {
        // return 0;
    }

    //return type
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    
    
    
    list_freeList(localVarHeaderType);
    
    free(localVarPath);
    if (localVarSingleItemJSON_data) {
        cJSON_Delete(localVarSingleItemJSON_data);
        localVarSingleItemJSON_data = NULL;
    }
    free(localVarBodyParameters);
    return elementToReturn;
end:
    free(localVarPath);
    return NULL;

}

// upload tgz file to upload task
//
// upload tgz file to upload task
//
api_upload_task_file_resp_t*
ClientAPI_uploadTaskFile(apiClient_t *apiClient, char *X_Token, char *task_id, binary_t* file)
{
    list_t    *localVarQueryParameters = NULL;
    list_t    *localVarHeaderParameters = list_createList();
    list_t    *localVarFormParameters = list_createList();
    list_t *localVarHeaderType = list_createList();
    list_t *localVarContentType = list_createList();
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v1/upload-tasks/{task_id}/tar")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v1/upload-tasks/{task_id}/tar");


    // Path Params
    long sizeOfPathParams_task_id = strlen(task_id)+3 + strlen("{ task_id }");
    if(task_id == NULL) {
        goto end;
    }
    char* localVarToReplace_task_id = malloc(sizeOfPathParams_task_id);
    sprintf(localVarToReplace_task_id, "{%s}", "task_id");

    localVarPath = strReplace(localVarPath, localVarToReplace_task_id, task_id);



    // header parameters
    char *keyHeader_X_Token = NULL;
    char * valueHeader_X_Token = 0;
    keyValuePair_t *keyPairHeader_X_Token = 0;
    if (X_Token) {
        keyHeader_X_Token = strdup("X-Token");
        valueHeader_X_Token = strdup((X_Token));
        keyPairHeader_X_Token = keyValuePair_create(keyHeader_X_Token, valueHeader_X_Token);
        list_addElement(localVarHeaderParameters,keyPairHeader_X_Token);
    }


    // form parameters
    char *keyForm_file = NULL;
    binary_t* valueForm_file = 0;
    keyValuePair_t *keyPairForm_file = 0;
    if (file != NULL)
    {
        keyForm_file = strdup("file");
        valueForm_file = file;
        keyPairForm_file = keyValuePair_create(keyForm_file, &valueForm_file);
        list_addElement(localVarFormParameters,keyPairForm_file); //file adding
    }
    list_addElement(localVarHeaderType,"*/*"); //produces
    list_addElement(localVarContentType,"multipart/form-data"); //consumes
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "PUT");

    // uncomment below to debug the error response
    //if (apiClient->response_code == 200) {
    //    printf("%s\n","OK");
    //}
    //nonprimitive not container
    cJSON *ClientAPIlocalVarJSON = cJSON_Parse(apiClient->dataReceived);
    api_upload_task_file_resp_t *elementToReturn = api_upload_task_file_resp_parseFromJSON(ClientAPIlocalVarJSON);
    cJSON_Delete(ClientAPIlocalVarJSON);
    if(elementToReturn == NULL) {
        // return 0;
    }

    //return type
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    
    list_freeList(localVarHeaderParameters);
    list_freeList(localVarFormParameters);
    list_freeList(localVarHeaderType);
    list_freeList(localVarContentType);
    free(localVarPath);
    free(localVarToReplace_task_id);
    if (keyHeader_X_Token) {
        free(keyHeader_X_Token);
        keyHeader_X_Token = NULL;
    }
    if (valueHeader_X_Token) {
        free(valueHeader_X_Token);
        valueHeader_X_Token = NULL;
    }
    free(keyPairHeader_X_Token);
    if (keyForm_file) {
        free(keyForm_file);
        keyForm_file = NULL;
    }
//    free(fileVar_file->data);
//    free(fileVar_file);
    free(keyPairForm_file);
    return elementToReturn;
end:
    free(localVarPath);
    return NULL;

}

// get upload task status
//
// get upload task status
//
upload_task_info_200_response_t*
ClientAPI_uploadTaskInfo(apiClient_t *apiClient, char *X_Token, char *task_id)
{
    list_t    *localVarQueryParameters = NULL;
    list_t    *localVarHeaderParameters = list_createList();
    list_t    *localVarFormParameters = NULL;
    list_t *localVarHeaderType = list_createList();
    list_t *localVarContentType = NULL;
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v1/upload-tasks/{task_id}/status")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v1/upload-tasks/{task_id}/status");


    // Path Params
    long sizeOfPathParams_task_id = strlen(task_id)+3 + strlen("{ task_id }");
    if(task_id == NULL) {
        goto end;
    }
    char* localVarToReplace_task_id = malloc(sizeOfPathParams_task_id);
    sprintf(localVarToReplace_task_id, "{%s}", "task_id");

    localVarPath = strReplace(localVarPath, localVarToReplace_task_id, task_id);



    // header parameters
    char *keyHeader_X_Token = NULL;
    char * valueHeader_X_Token = 0;
    keyValuePair_t *keyPairHeader_X_Token = 0;
    if (X_Token) {
        keyHeader_X_Token = strdup("X-Token");
        valueHeader_X_Token = strdup((X_Token));
        keyPairHeader_X_Token = keyValuePair_create(keyHeader_X_Token, valueHeader_X_Token);
        list_addElement(localVarHeaderParameters,keyPairHeader_X_Token);
    }

    list_addElement(localVarHeaderType,"*/*"); //produces
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "GET");

    // uncomment below to debug the error response
    //if (apiClient->response_code == 200) {
    //    printf("%s\n","OK");
    //}
    //nonprimitive not container
    cJSON *ClientAPIlocalVarJSON = cJSON_Parse(apiClient->dataReceived);
    upload_task_info_200_response_t *elementToReturn = upload_task_info_200_response_parseFromJSON(ClientAPIlocalVarJSON);
    cJSON_Delete(ClientAPIlocalVarJSON);
    if(elementToReturn == NULL) {
        // return 0;
    }

    //return type
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    
    list_freeList(localVarHeaderParameters);
    
    list_freeList(localVarHeaderType);
    
    free(localVarPath);
    free(localVarToReplace_task_id);
    if (keyHeader_X_Token) {
        free(keyHeader_X_Token);
        keyHeader_X_Token = NULL;
    }
    if (valueHeader_X_Token) {
        free(valueHeader_X_Token);
        valueHeader_X_Token = NULL;
    }
    free(keyPairHeader_X_Token);
    return elementToReturn;
end:
    free(localVarPath);
    return NULL;

}

// upload layer file to upload task
//
api_upload_task_layer_file_resp_t*
ClientAPI_uploadTaskLayerFile(apiClient_t *apiClient, char *X_Token, char *task_id, binary_t* file)
{
    list_t    *localVarQueryParameters = NULL;
    list_t    *localVarHeaderParameters = list_createList();
    list_t    *localVarFormParameters = list_createList();
    list_t *localVarHeaderType = list_createList();
    list_t *localVarContentType = list_createList();
    char      *localVarBodyParameters = NULL;

    // create the path
    long sizeOfPath = strlen("/api/v1/upload-tasks/{task_id}/layer")+1;
    char *localVarPath = malloc(sizeOfPath);
    snprintf(localVarPath, sizeOfPath, "/api/v1/upload-tasks/{task_id}/layer");


    // Path Params
    long sizeOfPathParams_task_id = strlen(task_id)+3 + strlen("{ task_id }");
    if(task_id == NULL) {
        goto end;
    }
    char* localVarToReplace_task_id = malloc(sizeOfPathParams_task_id);
    sprintf(localVarToReplace_task_id, "{%s}", "task_id");

    localVarPath = strReplace(localVarPath, localVarToReplace_task_id, task_id);



    // header parameters
    char *keyHeader_X_Token = NULL;
    char * valueHeader_X_Token = 0;
    keyValuePair_t *keyPairHeader_X_Token = 0;
    if (X_Token) {
        keyHeader_X_Token = strdup("X-Token");
        valueHeader_X_Token = strdup((X_Token));
        keyPairHeader_X_Token = keyValuePair_create(keyHeader_X_Token, valueHeader_X_Token);
        list_addElement(localVarHeaderParameters,keyPairHeader_X_Token);
    }


    // form parameters
    char *keyForm_file = NULL;
    binary_t* valueForm_file = 0;
    keyValuePair_t *keyPairForm_file = 0;
    if (file != NULL)
    {
        keyForm_file = strdup("file");
        valueForm_file = file;
        keyPairForm_file = keyValuePair_create(keyForm_file, &valueForm_file);
        list_addElement(localVarFormParameters,keyPairForm_file); //file adding
    }
    list_addElement(localVarHeaderType,"*/*"); //produces
    list_addElement(localVarContentType,"multipart/form-data"); //consumes
    apiClient_invoke(apiClient,
                    localVarPath,
                    localVarQueryParameters,
                    localVarHeaderParameters,
                    localVarFormParameters,
                    localVarHeaderType,
                    localVarContentType,
                    localVarBodyParameters,
                    "PUT");

    // uncomment below to debug the error response
    //if (apiClient->response_code == 200) {
    //    printf("%s\n","OK");
    //}
    //nonprimitive not container
    cJSON *ClientAPIlocalVarJSON = cJSON_Parse(apiClient->dataReceived);
    api_upload_task_layer_file_resp_t *elementToReturn = api_upload_task_layer_file_resp_parseFromJSON(ClientAPIlocalVarJSON);
    cJSON_Delete(ClientAPIlocalVarJSON);
    if(elementToReturn == NULL) {
        // return 0;
    }

    //return type
    if (apiClient->dataReceived) {
        free(apiClient->dataReceived);
        apiClient->dataReceived = NULL;
        apiClient->dataReceivedLen = 0;
    }
    
    list_freeList(localVarHeaderParameters);
    list_freeList(localVarFormParameters);
    list_freeList(localVarHeaderType);
    list_freeList(localVarContentType);
    free(localVarPath);
    free(localVarToReplace_task_id);
    if (keyHeader_X_Token) {
        free(keyHeader_X_Token);
        keyHeader_X_Token = NULL;
    }
    if (valueHeader_X_Token) {
        free(valueHeader_X_Token);
        valueHeader_X_Token = NULL;
    }
    free(keyPairHeader_X_Token);
    if (keyForm_file) {
        free(keyForm_file);
        keyForm_file = NULL;
    }
//    free(fileVar_file->data);
//    free(fileVar_file);
    free(keyPairForm_file);
    return elementToReturn;
end:
    free(localVarPath);
    return NULL;

}

