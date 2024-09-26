#ifndef fuzzy_search_app_200_response_TEST
#define fuzzy_search_app_200_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define fuzzy_search_app_200_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/fuzzy_search_app_200_response.h"
fuzzy_search_app_200_response_t* instantiate_fuzzy_search_app_200_response(int include_optional);



fuzzy_search_app_200_response_t* instantiate_fuzzy_search_app_200_response(int include_optional) {
  fuzzy_search_app_200_response_t* fuzzy_search_app_200_response = NULL;
  if (include_optional) {
    fuzzy_search_app_200_response = fuzzy_search_app_200_response_create(
      56,
      list_createList(),
      "0",
      "0"
    );
  } else {
    fuzzy_search_app_200_response = fuzzy_search_app_200_response_create(
      56,
      list_createList(),
      "0",
      "0"
    );
  }

  return fuzzy_search_app_200_response;
}


#ifdef fuzzy_search_app_200_response_MAIN

void test_fuzzy_search_app_200_response(int include_optional) {
    fuzzy_search_app_200_response_t* fuzzy_search_app_200_response_1 = instantiate_fuzzy_search_app_200_response(include_optional);

	cJSON* jsonfuzzy_search_app_200_response_1 = fuzzy_search_app_200_response_convertToJSON(fuzzy_search_app_200_response_1);
	printf("fuzzy_search_app_200_response :\n%s\n", cJSON_Print(jsonfuzzy_search_app_200_response_1));
	fuzzy_search_app_200_response_t* fuzzy_search_app_200_response_2 = fuzzy_search_app_200_response_parseFromJSON(jsonfuzzy_search_app_200_response_1);
	cJSON* jsonfuzzy_search_app_200_response_2 = fuzzy_search_app_200_response_convertToJSON(fuzzy_search_app_200_response_2);
	printf("repeating fuzzy_search_app_200_response:\n%s\n", cJSON_Print(jsonfuzzy_search_app_200_response_2));
}

int main() {
  test_fuzzy_search_app_200_response(1);
  test_fuzzy_search_app_200_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // fuzzy_search_app_200_response_MAIN
#endif // fuzzy_search_app_200_response_TEST
