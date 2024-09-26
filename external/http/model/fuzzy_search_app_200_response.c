#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fuzzy_search_app_200_response.h"



fuzzy_search_app_200_response_t *fuzzy_search_app_200_response_create(
    int code,
    list_t *data,
    char *msg,
    char *trace_id
    ) {
    fuzzy_search_app_200_response_t *fuzzy_search_app_200_response_local_var = malloc(sizeof(fuzzy_search_app_200_response_t));
    if (!fuzzy_search_app_200_response_local_var) {
        return NULL;
    }
    fuzzy_search_app_200_response_local_var->code = code;
    fuzzy_search_app_200_response_local_var->data = data;
    fuzzy_search_app_200_response_local_var->msg = msg;
    fuzzy_search_app_200_response_local_var->trace_id = trace_id;

    return fuzzy_search_app_200_response_local_var;
}


void fuzzy_search_app_200_response_free(fuzzy_search_app_200_response_t *fuzzy_search_app_200_response) {
    if(NULL == fuzzy_search_app_200_response){
        return ;
    }
    listEntry_t *listEntry;
    if (fuzzy_search_app_200_response->data) {
        list_ForEach(listEntry, fuzzy_search_app_200_response->data) {
            request_register_struct_free(listEntry->data);
        }
        list_freeList(fuzzy_search_app_200_response->data);
        fuzzy_search_app_200_response->data = NULL;
    }
    if (fuzzy_search_app_200_response->msg) {
        free(fuzzy_search_app_200_response->msg);
        fuzzy_search_app_200_response->msg = NULL;
    }
    if (fuzzy_search_app_200_response->trace_id) {
        free(fuzzy_search_app_200_response->trace_id);
        fuzzy_search_app_200_response->trace_id = NULL;
    }
    free(fuzzy_search_app_200_response);
}

cJSON *fuzzy_search_app_200_response_convertToJSON(fuzzy_search_app_200_response_t *fuzzy_search_app_200_response) {
    cJSON *item = cJSON_CreateObject();

    // fuzzy_search_app_200_response->code
    if(fuzzy_search_app_200_response->code) {
    if(cJSON_AddNumberToObject(item, "code", fuzzy_search_app_200_response->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // fuzzy_search_app_200_response->data
    if(fuzzy_search_app_200_response->data) {
    cJSON *data = cJSON_AddArrayToObject(item, "data");
    if(data == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *dataListEntry;
    if (fuzzy_search_app_200_response->data) {
    list_ForEach(dataListEntry, fuzzy_search_app_200_response->data) {
    cJSON *itemLocal = request_register_struct_convertToJSON(dataListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(data, itemLocal);
    }
    }
    }


    // fuzzy_search_app_200_response->msg
    if(fuzzy_search_app_200_response->msg) {
    if(cJSON_AddStringToObject(item, "msg", fuzzy_search_app_200_response->msg) == NULL) {
    goto fail; //String
    }
    }


    // fuzzy_search_app_200_response->trace_id
    if(fuzzy_search_app_200_response->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", fuzzy_search_app_200_response->trace_id) == NULL) {
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

fuzzy_search_app_200_response_t *fuzzy_search_app_200_response_parseFromJSON(cJSON *fuzzy_search_app_200_responseJSON){

    fuzzy_search_app_200_response_t *fuzzy_search_app_200_response_local_var = NULL;

    // define the local list for fuzzy_search_app_200_response->data
    list_t *dataList = NULL;

    // fuzzy_search_app_200_response->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(fuzzy_search_app_200_responseJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // fuzzy_search_app_200_response->data
    cJSON *data = cJSON_GetObjectItemCaseSensitive(fuzzy_search_app_200_responseJSON, "data");
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
        request_register_struct_t *dataItem = request_register_struct_parseFromJSON(data_local_nonprimitive);

        list_addElement(dataList, dataItem);
    }
    }

    // fuzzy_search_app_200_response->msg
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(fuzzy_search_app_200_responseJSON, "msg");
    if (msg) { 
    if(!cJSON_IsString(msg) && !cJSON_IsNull(msg))
    {
    goto end; //String
    }
    }

    // fuzzy_search_app_200_response->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(fuzzy_search_app_200_responseJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    fuzzy_search_app_200_response_local_var = fuzzy_search_app_200_response_create (
        code ? code->valuedouble : 0,
        data ? dataList : NULL,
        msg && !cJSON_IsNull(msg) ? strdup(msg->valuestring) : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return fuzzy_search_app_200_response_local_var;
end:
    if (dataList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, dataList) {
            request_register_struct_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(dataList);
        dataList = NULL;
    }
    return NULL;

}
