#ifndef response_upload_task_resp_TEST
#define response_upload_task_resp_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define response_upload_task_resp_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/response_upload_task_resp.h"
response_upload_task_resp_t* instantiate_response_upload_task_resp(int include_optional);



response_upload_task_resp_t* instantiate_response_upload_task_resp(int include_optional) {
  response_upload_task_resp_t* response_upload_task_resp = NULL;
  if (include_optional) {
    response_upload_task_resp = response_upload_task_resp_create(
      "0"
    );
  } else {
    response_upload_task_resp = response_upload_task_resp_create(
      "0"
    );
  }

  return response_upload_task_resp;
}


#ifdef response_upload_task_resp_MAIN

void test_response_upload_task_resp(int include_optional) {
    response_upload_task_resp_t* response_upload_task_resp_1 = instantiate_response_upload_task_resp(include_optional);

	cJSON* jsonresponse_upload_task_resp_1 = response_upload_task_resp_convertToJSON(response_upload_task_resp_1);
	printf("response_upload_task_resp :\n%s\n", cJSON_Print(jsonresponse_upload_task_resp_1));
	response_upload_task_resp_t* response_upload_task_resp_2 = response_upload_task_resp_parseFromJSON(jsonresponse_upload_task_resp_1);
	cJSON* jsonresponse_upload_task_resp_2 = response_upload_task_resp_convertToJSON(response_upload_task_resp_2);
	printf("repeating response_upload_task_resp:\n%s\n", cJSON_Print(jsonresponse_upload_task_resp_2));
}

int main() {
  test_response_upload_task_resp(1);
  test_response_upload_task_resp(0);

  printf("Hello world \n");
  return 0;
}

#endif // response_upload_task_resp_MAIN
#endif // response_upload_task_resp_TEST
