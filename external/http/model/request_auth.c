#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "request_auth.h"



request_auth_t *request_auth_create(
    char *password,
    char *username
    ) {
    request_auth_t *request_auth_local_var = malloc(sizeof(request_auth_t));
    if (!request_auth_local_var) {
        return NULL;
    }
    request_auth_local_var->password = password;
    request_auth_local_var->username = username;

    return request_auth_local_var;
}


void request_auth_free(request_auth_t *request_auth) {
    if(NULL == request_auth){
        return ;
    }
    listEntry_t *listEntry;
    if (request_auth->password) {
        free(request_auth->password);
        request_auth->password = NULL;
    }
    if (request_auth->username) {
        free(request_auth->username);
        request_auth->username = NULL;
    }
    free(request_auth);
}

cJSON *request_auth_convertToJSON(request_auth_t *request_auth) {
    cJSON *item = cJSON_CreateObject();

    // request_auth->password
    if(request_auth->password) {
    if(cJSON_AddStringToObject(item, "password", request_auth->password) == NULL) {
    goto fail; //String
    }
    }


    // request_auth->username
    if(request_auth->username) {
    if(cJSON_AddStringToObject(item, "username", request_auth->username) == NULL) {
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

request_auth_t *request_auth_parseFromJSON(cJSON *request_authJSON){

    request_auth_t *request_auth_local_var = NULL;

    // request_auth->password
    cJSON *password = cJSON_GetObjectItemCaseSensitive(request_authJSON, "password");
    if (password) { 
    if(!cJSON_IsString(password) && !cJSON_IsNull(password))
    {
    goto end; //String
    }
    }

    // request_auth->username
    cJSON *username = cJSON_GetObjectItemCaseSensitive(request_authJSON, "username");
    if (username) { 
    if(!cJSON_IsString(username) && !cJSON_IsNull(username))
    {
    goto end; //String
    }
    }


    request_auth_local_var = request_auth_create (
        password && !cJSON_IsNull(password) ? strdup(password->valuestring) : NULL,
        username && !cJSON_IsNull(username) ? strdup(username->valuestring) : NULL
        );

    return request_auth_local_var;
end:
    return NULL;

}
