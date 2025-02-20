#ifndef _api_v1_repos_get_200_response_TEST
#define _api_v1_repos_get_200_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define _api_v1_repos_get_200_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/_api_v1_repos_get_200_response.h"
_api_v1_repos_get_200_response_t* instantiate__api_v1_repos_get_200_response(int include_optional);



_api_v1_repos_get_200_response_t* instantiate__api_v1_repos_get_200_response(int include_optional) {
  _api_v1_repos_get_200_response_t* _api_v1_repos_get_200_response = NULL;
  if (include_optional) {
    _api_v1_repos_get_200_response = _api_v1_repos_get_200_response_create(
      56,
      list_createList(),
      "0",
      "0"
    );
  } else {
    _api_v1_repos_get_200_response = _api_v1_repos_get_200_response_create(
      56,
      list_createList(),
      "0",
      "0"
    );
  }

  return _api_v1_repos_get_200_response;
}


#ifdef _api_v1_repos_get_200_response_MAIN

void test__api_v1_repos_get_200_response(int include_optional) {
    _api_v1_repos_get_200_response_t* _api_v1_repos_get_200_response_1 = instantiate__api_v1_repos_get_200_response(include_optional);

	cJSON* json_api_v1_repos_get_200_response_1 = _api_v1_repos_get_200_response_convertToJSON(_api_v1_repos_get_200_response_1);
	printf("_api_v1_repos_get_200_response :\n%s\n", cJSON_Print(json_api_v1_repos_get_200_response_1));
	_api_v1_repos_get_200_response_t* _api_v1_repos_get_200_response_2 = _api_v1_repos_get_200_response_parseFromJSON(json_api_v1_repos_get_200_response_1);
	cJSON* json_api_v1_repos_get_200_response_2 = _api_v1_repos_get_200_response_convertToJSON(_api_v1_repos_get_200_response_2);
	printf("repeating _api_v1_repos_get_200_response:\n%s\n", cJSON_Print(json_api_v1_repos_get_200_response_2));
}

int main() {
  test__api_v1_repos_get_200_response(1);
  test__api_v1_repos_get_200_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // _api_v1_repos_get_200_response_MAIN
#endif // _api_v1_repos_get_200_response_TEST
