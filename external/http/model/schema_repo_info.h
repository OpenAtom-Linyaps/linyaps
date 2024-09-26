/*
 * schema_repo_info.h
 *
 * 
 */

#ifndef _schema_repo_info_H_
#define _schema_repo_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct schema_repo_info_t schema_repo_info_t;




typedef struct schema_repo_info_t {
    char *mode; // string
    char *name; // string
    list_t *refs; //primitive container

} schema_repo_info_t;

schema_repo_info_t *schema_repo_info_create(
    char *mode,
    char *name,
    list_t *refs
);

void schema_repo_info_free(schema_repo_info_t *schema_repo_info);

schema_repo_info_t *schema_repo_info_parseFromJSON(cJSON *schema_repo_infoJSON);

cJSON *schema_repo_info_convertToJSON(schema_repo_info_t *schema_repo_info);

#endif /* _schema_repo_info_H_ */

