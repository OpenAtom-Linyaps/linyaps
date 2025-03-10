#ifndef upload_task_info_200_response_TEST
#define upload_task_info_200_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define upload_task_info_200_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/upload_task_info_200_response.h"
upload_task_info_200_response_t* instantiate_upload_task_info_200_response(int include_optional);

#include "test_response_upload_task_status_info.c"


upload_task_info_200_response_t* instantiate_upload_task_info_200_response(int include_optional) {
  upload_task_info_200_response_t* upload_task_info_200_response = NULL;
  if (include_optional) {
    upload_task_info_200_response = upload_task_info_200_response_create(
      56,
       // false, not to have infinite recursion
      instantiate_response_upload_task_status_info(0),
      "0",
      "0"
    );
  } else {
    upload_task_info_200_response = upload_task_info_200_response_create(
      56,
      NULL,
      "0",
      "0"
    );
  }

  return upload_task_info_200_response;
}


#ifdef upload_task_info_200_response_MAIN

void test_upload_task_info_200_response(int include_optional) {
    upload_task_info_200_response_t* upload_task_info_200_response_1 = instantiate_upload_task_info_200_response(include_optional);

	cJSON* jsonupload_task_info_200_response_1 = upload_task_info_200_response_convertToJSON(upload_task_info_200_response_1);
	printf("upload_task_info_200_response :\n%s\n", cJSON_Print(jsonupload_task_info_200_response_1));
	upload_task_info_200_response_t* upload_task_info_200_response_2 = upload_task_info_200_response_parseFromJSON(jsonupload_task_info_200_response_1);
	cJSON* jsonupload_task_info_200_response_2 = upload_task_info_200_response_convertToJSON(upload_task_info_200_response_2);
	printf("repeating upload_task_info_200_response:\n%s\n", cJSON_Print(jsonupload_task_info_200_response_2));
}

int main() {
  test_upload_task_info_200_response(1);
  test_upload_task_info_200_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // upload_task_info_200_response_MAIN
#endif // upload_task_info_200_response_TEST
