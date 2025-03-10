#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "request_fuzzy_search_req.h"



request_fuzzy_search_req_t *request_fuzzy_search_req_create(
    char *channel,
    char *app_id,
    char *arch,
    char *repo_name,
    char *version
    ) {
    request_fuzzy_search_req_t *request_fuzzy_search_req_local_var = malloc(sizeof(request_fuzzy_search_req_t));
    if (!request_fuzzy_search_req_local_var) {
        return NULL;
    }
    request_fuzzy_search_req_local_var->channel = channel;
    request_fuzzy_search_req_local_var->app_id = app_id;
    request_fuzzy_search_req_local_var->arch = arch;
    request_fuzzy_search_req_local_var->repo_name = repo_name;
    request_fuzzy_search_req_local_var->version = version;

    return request_fuzzy_search_req_local_var;
}


void request_fuzzy_search_req_free(request_fuzzy_search_req_t *request_fuzzy_search_req) {
    if(NULL == request_fuzzy_search_req){
        return ;
    }
    listEntry_t *listEntry;
    if (request_fuzzy_search_req->channel) {
        free(request_fuzzy_search_req->channel);
        request_fuzzy_search_req->channel = NULL;
    }
    if (request_fuzzy_search_req->app_id) {
        free(request_fuzzy_search_req->app_id);
        request_fuzzy_search_req->app_id = NULL;
    }
    if (request_fuzzy_search_req->arch) {
        free(request_fuzzy_search_req->arch);
        request_fuzzy_search_req->arch = NULL;
    }
    if (request_fuzzy_search_req->repo_name) {
        free(request_fuzzy_search_req->repo_name);
        request_fuzzy_search_req->repo_name = NULL;
    }
    if (request_fuzzy_search_req->version) {
        free(request_fuzzy_search_req->version);
        request_fuzzy_search_req->version = NULL;
    }
    free(request_fuzzy_search_req);
}

cJSON *request_fuzzy_search_req_convertToJSON(request_fuzzy_search_req_t *request_fuzzy_search_req) {
    cJSON *item = cJSON_CreateObject();

    // request_fuzzy_search_req->channel
    if(request_fuzzy_search_req->channel) {
    if(cJSON_AddStringToObject(item, "channel", request_fuzzy_search_req->channel) == NULL) {
    goto fail; //String
    }
    }


    // request_fuzzy_search_req->app_id
    if(request_fuzzy_search_req->app_id) {
    if(cJSON_AddStringToObject(item, "appId", request_fuzzy_search_req->app_id) == NULL) {
    goto fail; //String
    }
    }


    // request_fuzzy_search_req->arch
    if(request_fuzzy_search_req->arch) {
    if(cJSON_AddStringToObject(item, "arch", request_fuzzy_search_req->arch) == NULL) {
    goto fail; //String
    }
    }


    // request_fuzzy_search_req->repo_name
    if(request_fuzzy_search_req->repo_name) {
    if(cJSON_AddStringToObject(item, "repoName", request_fuzzy_search_req->repo_name) == NULL) {
    goto fail; //String
    }
    }


    // request_fuzzy_search_req->version
    if(request_fuzzy_search_req->version) {
    if(cJSON_AddStringToObject(item, "version", request_fuzzy_search_req->version) == NULL) {
    goto fail; //String
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

request_fuzzy_search_req_t *request_fuzzy_search_req_parseFromJSON(cJSON *request_fuzzy_search_reqJSON){

    request_fuzzy_search_req_t *request_fuzzy_search_req_local_var = NULL;

    // request_fuzzy_search_req->channel
    cJSON *channel = cJSON_GetObjectItemCaseSensitive(request_fuzzy_search_reqJSON, "channel");
    if (channel) { 
    if(!cJSON_IsString(channel) && !cJSON_IsNull(channel))
    {
    goto end; //String
    }
    }

    // request_fuzzy_search_req->app_id
    cJSON *app_id = cJSON_GetObjectItemCaseSensitive(request_fuzzy_search_reqJSON, "appId");
    if (app_id) { 
    if(!cJSON_IsString(app_id) && !cJSON_IsNull(app_id))
    {
    goto end; //String
    }
    }

    // request_fuzzy_search_req->arch
    cJSON *arch = cJSON_GetObjectItemCaseSensitive(request_fuzzy_search_reqJSON, "arch");
    if (arch) { 
    if(!cJSON_IsString(arch) && !cJSON_IsNull(arch))
    {
    goto end; //String
    }
    }

    // request_fuzzy_search_req->repo_name
    cJSON *repo_name = cJSON_GetObjectItemCaseSensitive(request_fuzzy_search_reqJSON, "repoName");
    if (repo_name) { 
    if(!cJSON_IsString(repo_name) && !cJSON_IsNull(repo_name))
    {
    goto end; //String
    }
    }

    // request_fuzzy_search_req->version
    cJSON *version = cJSON_GetObjectItemCaseSensitive(request_fuzzy_search_reqJSON, "version");
    if (version) { 
    if(!cJSON_IsString(version) && !cJSON_IsNull(version))
    {
    goto end; //String
    }
    }


    request_fuzzy_search_req_local_var = request_fuzzy_search_req_create (
        channel && !cJSON_IsNull(channel) ? strdup(channel->valuestring) : NULL,
        app_id && !cJSON_IsNull(app_id) ? strdup(app_id->valuestring) : NULL,
        arch && !cJSON_IsNull(arch) ? strdup(arch->valuestring) : NULL,
        repo_name && !cJSON_IsNull(repo_name) ? strdup(repo_name->valuestring) : NULL,
        version && !cJSON_IsNull(version) ? strdup(version->valuestring) : NULL
        );

    return request_fuzzy_search_req_local_var;
end:
    return NULL;

}
