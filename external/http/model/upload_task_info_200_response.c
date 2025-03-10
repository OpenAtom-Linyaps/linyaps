#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "upload_task_info_200_response.h"



upload_task_info_200_response_t *upload_task_info_200_response_create(
    int code,
    response_upload_task_status_info_t *data,
    char *msg,
    char *trace_id
    ) {
    upload_task_info_200_response_t *upload_task_info_200_response_local_var = malloc(sizeof(upload_task_info_200_response_t));
    if (!upload_task_info_200_response_local_var) {
        return NULL;
    }
    upload_task_info_200_response_local_var->code = code;
    upload_task_info_200_response_local_var->data = data;
    upload_task_info_200_response_local_var->msg = msg;
    upload_task_info_200_response_local_var->trace_id = trace_id;

    return upload_task_info_200_response_local_var;
}


void upload_task_info_200_response_free(upload_task_info_200_response_t *upload_task_info_200_response) {
    if(NULL == upload_task_info_200_response){
        return ;
    }
    listEntry_t *listEntry;
    if (upload_task_info_200_response->data) {
        response_upload_task_status_info_free(upload_task_info_200_response->data);
        upload_task_info_200_response->data = NULL;
    }
    if (upload_task_info_200_response->msg) {
        free(upload_task_info_200_response->msg);
        upload_task_info_200_response->msg = NULL;
    }
    if (upload_task_info_200_response->trace_id) {
        free(upload_task_info_200_response->trace_id);
        upload_task_info_200_response->trace_id = NULL;
    }
    free(upload_task_info_200_response);
}

cJSON *upload_task_info_200_response_convertToJSON(upload_task_info_200_response_t *upload_task_info_200_response) {
    cJSON *item = cJSON_CreateObject();

    // upload_task_info_200_response->code
    if(upload_task_info_200_response->code) {
    if(cJSON_AddNumberToObject(item, "code", upload_task_info_200_response->code) == NULL) {
    goto fail; //Numeric
    }
    }


    // upload_task_info_200_response->data
    if(upload_task_info_200_response->data) {
    cJSON *data_local_JSON = response_upload_task_status_info_convertToJSON(upload_task_info_200_response->data);
    if(data_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "data", data_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // upload_task_info_200_response->msg
    if(upload_task_info_200_response->msg) {
    if(cJSON_AddStringToObject(item, "msg", upload_task_info_200_response->msg) == NULL) {
    goto fail; //String
    }
    }


    // upload_task_info_200_response->trace_id
    if(upload_task_info_200_response->trace_id) {
    if(cJSON_AddStringToObject(item, "trace_id", upload_task_info_200_response->trace_id) == NULL) {
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

upload_task_info_200_response_t *upload_task_info_200_response_parseFromJSON(cJSON *upload_task_info_200_responseJSON){

    upload_task_info_200_response_t *upload_task_info_200_response_local_var = NULL;

    // define the local variable for upload_task_info_200_response->data
    response_upload_task_status_info_t *data_local_nonprim = NULL;

    // upload_task_info_200_response->code
    cJSON *code = cJSON_GetObjectItemCaseSensitive(upload_task_info_200_responseJSON, "code");
    if (code) { 
    if(!cJSON_IsNumber(code))
    {
    goto end; //Numeric
    }
    }

    // upload_task_info_200_response->data
    cJSON *data = cJSON_GetObjectItemCaseSensitive(upload_task_info_200_responseJSON, "data");
    if (data) { 
    data_local_nonprim = response_upload_task_status_info_parseFromJSON(data); //nonprimitive
    }

    // upload_task_info_200_response->msg
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(upload_task_info_200_responseJSON, "msg");
    if (msg) { 
    if(!cJSON_IsString(msg) && !cJSON_IsNull(msg))
    {
    goto end; //String
    }
    }

    // upload_task_info_200_response->trace_id
    cJSON *trace_id = cJSON_GetObjectItemCaseSensitive(upload_task_info_200_responseJSON, "trace_id");
    if (trace_id) { 
    if(!cJSON_IsString(trace_id) && !cJSON_IsNull(trace_id))
    {
    goto end; //String
    }
    }


    upload_task_info_200_response_local_var = upload_task_info_200_response_create (
        code ? code->valuedouble : 0,
        data ? data_local_nonprim : NULL,
        msg && !cJSON_IsNull(msg) ? strdup(msg->valuestring) : NULL,
        trace_id && !cJSON_IsNull(trace_id) ? strdup(trace_id->valuestring) : NULL
        );

    return upload_task_info_200_response_local_var;
end:
    if (data_local_nonprim) {
        response_upload_task_status_info_free(data_local_nonprim);
        data_local_nonprim = NULL;
    }
    return NULL;

}
