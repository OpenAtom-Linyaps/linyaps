#ifndef get_repo_200_response_TEST
#define get_repo_200_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define get_repo_200_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/get_repo_200_response.h"
get_repo_200_response_t* instantiate_get_repo_200_response(int include_optional);

#include "test_schema_repo_info.c"


get_repo_200_response_t* instantiate_get_repo_200_response(int include_optional) {
  get_repo_200_response_t* get_repo_200_response = NULL;
  if (include_optional) {
    get_repo_200_response = get_repo_200_response_create(
      56,
       // false, not to have infinite recursion
      instantiate_schema_repo_info(0),
      "0",
      "0"
    );
  } else {
    get_repo_200_response = get_repo_200_response_create(
      56,
      NULL,
      "0",
      "0"
    );
  }

  return get_repo_200_response;
}


#ifdef get_repo_200_response_MAIN

void test_get_repo_200_response(int include_optional) {
    get_repo_200_response_t* get_repo_200_response_1 = instantiate_get_repo_200_response(include_optional);

	cJSON* jsonget_repo_200_response_1 = get_repo_200_response_convertToJSON(get_repo_200_response_1);
	printf("get_repo_200_response :\n%s\n", cJSON_Print(jsonget_repo_200_response_1));
	get_repo_200_response_t* get_repo_200_response_2 = get_repo_200_response_parseFromJSON(jsonget_repo_200_response_1);
	cJSON* jsonget_repo_200_response_2 = get_repo_200_response_convertToJSON(get_repo_200_response_2);
	printf("repeating get_repo_200_response:\n%s\n", cJSON_Print(jsonget_repo_200_response_2));
}

int main() {
  test_get_repo_200_response(1);
  test_get_repo_200_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // get_repo_200_response_MAIN
#endif // get_repo_200_response_TEST
