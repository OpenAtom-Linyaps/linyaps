#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "request_register_struct.h"



request_register_struct_t *request_register_struct_create(
    char *app_id,
    char *arch,
    char *channel,
    char *description,
    char *kind,
    char *module,
    char *name,
    char *repo_name,
    char *runtime,
    int size,
    char *uab_url,
    char *version
    ) {
    request_register_struct_t *request_register_struct_local_var = malloc(sizeof(request_register_struct_t));
    if (!request_register_struct_local_var) {
        return NULL;
    }
    request_register_struct_local_var->app_id = app_id;
    request_register_struct_local_var->arch = arch;
    request_register_struct_local_var->channel = channel;
    request_register_struct_local_var->description = description;
    request_register_struct_local_var->kind = kind;
    request_register_struct_local_var->module = module;
    request_register_struct_local_var->name = name;
    request_register_struct_local_var->repo_name = repo_name;
    request_register_struct_local_var->runtime = runtime;
    request_register_struct_local_var->size = size;
    request_register_struct_local_var->uab_url = uab_url;
    request_register_struct_local_var->version = version;

    return request_register_struct_local_var;
}


void request_register_struct_free(request_register_struct_t *request_register_struct) {
    if(NULL == request_register_struct){
        return ;
    }
    listEntry_t *listEntry;
    if (request_register_struct->app_id) {
        free(request_register_struct->app_id);
        request_register_struct->app_id = NULL;
    }
    if (request_register_struct->arch) {
        free(request_register_struct->arch);
        request_register_struct->arch = NULL;
    }
    if (request_register_struct->channel) {
        free(request_register_struct->channel);
        request_register_struct->channel = NULL;
    }
    if (request_register_struct->description) {
        free(request_register_struct->description);
        request_register_struct->description = NULL;
    }
    if (request_register_struct->kind) {
        free(request_register_struct->kind);
        request_register_struct->kind = NULL;
    }
    if (request_register_struct->module) {
        free(request_register_struct->module);
        request_register_struct->module = NULL;
    }
    if (request_register_struct->name) {
        free(request_register_struct->name);
        request_register_struct->name = NULL;
    }
    if (request_register_struct->repo_name) {
        free(request_register_struct->repo_name);
        request_register_struct->repo_name = NULL;
    }
    if (request_register_struct->runtime) {
        free(request_register_struct->runtime);
        request_register_struct->runtime = NULL;
    }
    if (request_register_struct->uab_url) {
        free(request_register_struct->uab_url);
        request_register_struct->uab_url = NULL;
    }
    if (request_register_struct->version) {
        free(request_register_struct->version);
        request_register_struct->version = NULL;
    }
    free(request_register_struct);
}

cJSON *request_register_struct_convertToJSON(request_register_struct_t *request_register_struct) {
    cJSON *item = cJSON_CreateObject();

    // request_register_struct->app_id
    if(request_register_struct->app_id) {
    if(cJSON_AddStringToObject(item, "appId", request_register_struct->app_id) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->arch
    if(request_register_struct->arch) {
    if(cJSON_AddStringToObject(item, "arch", request_register_struct->arch) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->channel
    if(request_register_struct->channel) {
    if(cJSON_AddStringToObject(item, "channel", request_register_struct->channel) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->description
    if(request_register_struct->description) {
    if(cJSON_AddStringToObject(item, "description", request_register_struct->description) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->kind
    if(request_register_struct->kind) {
    if(cJSON_AddStringToObject(item, "kind", request_register_struct->kind) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->module
    if(request_register_struct->module) {
    if(cJSON_AddStringToObject(item, "module", request_register_struct->module) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->name
    if(request_register_struct->name) {
    if(cJSON_AddStringToObject(item, "name", request_register_struct->name) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->repo_name
    if(request_register_struct->repo_name) {
    if(cJSON_AddStringToObject(item, "repoName", request_register_struct->repo_name) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->runtime
    if(request_register_struct->runtime) {
    if(cJSON_AddStringToObject(item, "runtime", request_register_struct->runtime) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->size
    if(request_register_struct->size) {
    if(cJSON_AddNumberToObject(item, "size", request_register_struct->size) == NULL) {
    goto fail; //Numeric
    }
    }


    // request_register_struct->uab_url
    if(request_register_struct->uab_url) {
    if(cJSON_AddStringToObject(item, "uabUrl", request_register_struct->uab_url) == NULL) {
    goto fail; //String
    }
    }


    // request_register_struct->version
    if(request_register_struct->version) {
    if(cJSON_AddStringToObject(item, "version", request_register_struct->version) == NULL) {
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

request_register_struct_t *request_register_struct_parseFromJSON(cJSON *request_register_structJSON){

    request_register_struct_t *request_register_struct_local_var = NULL;

    // request_register_struct->app_id
    cJSON *app_id = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "appId");
    if (app_id) { 
    if(!cJSON_IsString(app_id) && !cJSON_IsNull(app_id))
    {
    goto end; //String
    }
    }

    // request_register_struct->arch
    cJSON *arch = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "arch");
    if (arch) { 
    if(!cJSON_IsString(arch) && !cJSON_IsNull(arch))
    {
    goto end; //String
    }
    }

    // request_register_struct->channel
    cJSON *channel = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "channel");
    if (channel) { 
    if(!cJSON_IsString(channel) && !cJSON_IsNull(channel))
    {
    goto end; //String
    }
    }

    // request_register_struct->description
    cJSON *description = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "description");
    if (description) { 
    if(!cJSON_IsString(description) && !cJSON_IsNull(description))
    {
    goto end; //String
    }
    }

    // request_register_struct->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "kind");
    if (kind) { 
    if(!cJSON_IsString(kind) && !cJSON_IsNull(kind))
    {
    goto end; //String
    }
    }

    // request_register_struct->module
    cJSON *module = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "module");
    if (module) { 
    if(!cJSON_IsString(module) && !cJSON_IsNull(module))
    {
    goto end; //String
    }
    }

    // request_register_struct->name
    cJSON *name = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "name");
    if (name) { 
    if(!cJSON_IsString(name) && !cJSON_IsNull(name))
    {
    goto end; //String
    }
    }

    // request_register_struct->repo_name
    cJSON *repo_name = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "repoName");
    if (repo_name) { 
    if(!cJSON_IsString(repo_name) && !cJSON_IsNull(repo_name))
    {
    goto end; //String
    }
    }

    // request_register_struct->runtime
    cJSON *runtime = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "runtime");
    if (runtime) { 
    if(!cJSON_IsString(runtime) && !cJSON_IsNull(runtime))
    {
    goto end; //String
    }
    }

    // request_register_struct->size
    cJSON *size = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "size");
    if (size) { 
    if(!cJSON_IsNumber(size))
    {
    goto end; //Numeric
    }
    }

    // request_register_struct->uab_url
    cJSON *uab_url = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "uabUrl");
    if (uab_url) { 
    if(!cJSON_IsString(uab_url) && !cJSON_IsNull(uab_url))
    {
    goto end; //String
    }
    }

    // request_register_struct->version
    cJSON *version = cJSON_GetObjectItemCaseSensitive(request_register_structJSON, "version");
    if (version) { 
    if(!cJSON_IsString(version) && !cJSON_IsNull(version))
    {
    goto end; //String
    }
    }


    request_register_struct_local_var = request_register_struct_create (
        app_id && !cJSON_IsNull(app_id) ? strdup(app_id->valuestring) : NULL,
        arch && !cJSON_IsNull(arch) ? strdup(arch->valuestring) : NULL,
        channel && !cJSON_IsNull(channel) ? strdup(channel->valuestring) : NULL,
        description && !cJSON_IsNull(description) ? strdup(description->valuestring) : NULL,
        kind && !cJSON_IsNull(kind) ? strdup(kind->valuestring) : NULL,
        module && !cJSON_IsNull(module) ? strdup(module->valuestring) : NULL,
        name && !cJSON_IsNull(name) ? strdup(name->valuestring) : NULL,
        repo_name && !cJSON_IsNull(repo_name) ? strdup(repo_name->valuestring) : NULL,
        runtime && !cJSON_IsNull(runtime) ? strdup(runtime->valuestring) : NULL,
        size ? size->valuedouble : 0,
        uab_url && !cJSON_IsNull(uab_url) ? strdup(uab_url->valuestring) : NULL,
        version && !cJSON_IsNull(version) ? strdup(version->valuestring) : NULL
        );

    return request_register_struct_local_var;
end:
    return NULL;

}
