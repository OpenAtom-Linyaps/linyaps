#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "apiv2_search_app_response.h"



apiv2_search_app_response_t *apiv2_search_app_response_create(
    int code,
    list_t *data,
    char *trace_id
    ) {
    apiv2_search_app_response_t *apiv2_search_app_response_local_var = malloc(sizeof(apiv2_search_app_response_t));
    if (!apiv2_search_app_response_local_var) {
        return NULL;
    }
    apiv2_search_app_response_local_var->code = code;
    apiv2_search_app_response_local_var->data = data;
    apiv2_search_app_response_local_var->trace_id = trace_id;

    return apiv2_search_app_response_local_var;
}


void apiv2_search_app_response_free(apiv2_search_app_response_t *apiv2_search_app_response) {
    if(NULL == apiv2_search_app_response){
        return ;
    }
    listEntry_t *listEntry;
    if (apiv2_search_app_response->data) {
        list_ForEach(listEntry, apiv2_search_app_response->data) {
            request_register_struct_free(listEntry->data);
        }
        list_freeList(apiv2_search_app_response->data);
        apiv2_search_app_response->data = NULL;
    }
    if (apiv2_search_app_response->trace_id) {
        free(apiv2_search_app_response->trace_id);
        apiv2_search_app_response->trace_id = NULL;
    }
    free(apiv2_search_app_response);
}

cJSON *apiv2_search_app_response_convertToJSON(apiv2_search_app_response_t *apiv2_search_app_response) {
    cJSON *item = cJSON_CreateObject();

    // apiv2_search_app_response->code
    if(apiv2_search_app_response->code) {
    if(cJSON_AddNumberToObject(item, "code", apiv2_search_app_response->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // apiv2_search_app_response->data
    if(apiv2_search_app_response->data) {
    cJSON *data = cJSON_AddArrayToObject(item, "data");
    if(data == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *dataListEntry;
    if (apiv2_search_app_response->data) {
    list_ForEach(dataListEntry, apiv2_search_app_response->data) {
    cJSON *itemLocal = request_register_struct_convertToJSON(dataListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(data, itemLocal);
    }
    }
    }


    // apiv2_search_app_response->trace_id
    if(apiv2_search_app_response->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", apiv2_search_app_response->trace_id) == NULL) {
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

apiv2_search_app_response_t *apiv2_search_app_response_parseFromJSON(cJSON *apiv2_search_app_responseJSON){

    apiv2_search_app_response_t *apiv2_search_app_response_local_var = NULL;

    // define the local list for apiv2_search_app_response->data
    list_t *dataList = NULL;

    // apiv2_search_app_response->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(apiv2_search_app_responseJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // apiv2_search_app_response->data
    cJSON *data = cJSON_GetObjectItemCaseSensitive(apiv2_search_app_responseJSON, "data");
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

    // apiv2_search_app_response->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(apiv2_search_app_responseJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    apiv2_search_app_response_local_var = apiv2_search_app_response_create (
        code ? code->valuedouble : 0,
        data ? dataList : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return apiv2_search_app_response_local_var;
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
