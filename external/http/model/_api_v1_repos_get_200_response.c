#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "_api_v1_repos_get_200_response.h"



_api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response_create(
    int code,
    list_t *data,
    char *msg,
    char *trace_id
    ) {
    _api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response_local_var = malloc(sizeof(_api_v1_repos_get_200_response_t));
    if (!_api_v1_repos_get_200_response_local_var) {
        return NULL;
    }
    _api_v1_repos_get_200_response_local_var->code = code;
    _api_v1_repos_get_200_response_local_var->data = data;
    _api_v1_repos_get_200_response_local_var->msg = msg;
    _api_v1_repos_get_200_response_local_var->trace_id = trace_id;

    return _api_v1_repos_get_200_response_local_var;
}


void _api_v1_repos_get_200_response_free(_api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response) {
    if(NULL == _api_v1_repos_get_200_response){
        return ;
    }
    listEntry_t *listEntry;
    if (_api_v1_repos_get_200_response->data) {
        list_ForEach(listEntry, _api_v1_repos_get_200_response->data) {
            schema_repo_info_free(listEntry->data);
        }
        list_freeList(_api_v1_repos_get_200_response->data);
        _api_v1_repos_get_200_response->data = NULL;
    }
    if (_api_v1_repos_get_200_response->msg) {
        free(_api_v1_repos_get_200_response->msg);
        _api_v1_repos_get_200_response->msg = NULL;
    }
    if (_api_v1_repos_get_200_response->trace_id) {
        free(_api_v1_repos_get_200_response->trace_id);
        _api_v1_repos_get_200_response->trace_id = NULL;
    }
    free(_api_v1_repos_get_200_response);
}

cJSON *_api_v1_repos_get_200_response_convertToJSON(_api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response) {
    cJSON *item = cJSON_CreateObject();

    // _api_v1_repos_get_200_response->code
    if(_api_v1_repos_get_200_response->code) {
    if(cJSON_AddNumberToObject(item, "code", _api_v1_repos_get_200_response->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // _api_v1_repos_get_200_response->data
    if(_api_v1_repos_get_200_response->data) {
    cJSON *data = cJSON_AddArrayToObject(item, "data");
    if(data == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *dataListEntry;
    if (_api_v1_repos_get_200_response->data) {
    list_ForEach(dataListEntry, _api_v1_repos_get_200_response->data) {
    cJSON *itemLocal = schema_repo_info_convertToJSON(dataListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(data, itemLocal);
    }
    }
    }


    // _api_v1_repos_get_200_response->msg
    if(_api_v1_repos_get_200_response->msg) {
    if(cJSON_AddStringToObject(item, "msg", _api_v1_repos_get_200_response->msg) == NULL) {
    goto fail; //String
    }
    }


    // _api_v1_repos_get_200_response->trace_id
    if(_api_v1_repos_get_200_response->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", _api_v1_repos_get_200_response->trace_id) == NULL) {
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

_api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response_parseFromJSON(cJSON *_api_v1_repos_get_200_responseJSON){

    _api_v1_repos_get_200_response_t *_api_v1_repos_get_200_response_local_var = NULL;

    // define the local list for _api_v1_repos_get_200_response->data
    list_t *dataList = NULL;

    // _api_v1_repos_get_200_response->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(_api_v1_repos_get_200_responseJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // _api_v1_repos_get_200_response->data
    cJSON *data = cJSON_GetObjectItemCaseSensitive(_api_v1_repos_get_200_responseJSON, "data");
    if (data) { 
    cJSON *data_local_nonprimitive = NULL;
    if(!cJSON_IsArray(data)){
        goto end; //nonprimitive container
    }

    dataList = list_createList();

    cJSON_ArrayForEach(data_local_nonprimitive,data )
    {
        if(!cJSON_IsObject(data_local_nonprimitive)){
            goto end;
        }
        schema_repo_info_t *dataItem = schema_repo_info_parseFromJSON(data_local_nonprimitive);

        list_addElement(dataList, dataItem);
    }
    }

    // _api_v1_repos_get_200_response->msg
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(_api_v1_repos_get_200_responseJSON, "msg");
    if (msg) { 
    if(!cJSON_IsString(msg) && !cJSON_IsNull(msg))
    {
    goto end; //String
    }
    }

    // _api_v1_repos_get_200_response->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(_api_v1_repos_get_200_responseJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    _api_v1_repos_get_200_response_local_var = _api_v1_repos_get_200_response_create (
        code ? code->valuedouble : 0,
        data ? dataList : NULL,
        msg && !cJSON_IsNull(msg) ? strdup(msg->valuestring) : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return _api_v1_repos_get_200_response_local_var;
end:
    if (dataList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, dataList) {
            schema_repo_info_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(dataList);
        dataList = NULL;
    }
    return NULL;

}
