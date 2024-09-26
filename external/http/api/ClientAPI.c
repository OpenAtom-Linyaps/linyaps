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

