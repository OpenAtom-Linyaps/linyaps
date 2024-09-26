#ifndef schema_repo_info_TEST
#define schema_repo_info_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define schema_repo_info_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/schema_repo_info.h"
schema_repo_info_t* instantiate_schema_repo_info(int include_optional);



schema_repo_info_t* instantiate_schema_repo_info(int include_optional) {
  schema_repo_info_t* schema_repo_info = NULL;
  if (include_optional) {
    schema_repo_info = schema_repo_info_create(
      "0",
      "0",
      list_createList()
    );
  } else {
    schema_repo_info = schema_repo_info_create(
      "0",
      "0",
      list_createList()
    );
  }

  return schema_repo_info;
}


#ifdef schema_repo_info_MAIN

void test_schema_repo_info(int include_optional) {
    schema_repo_info_t* schema_repo_info_1 = instantiate_schema_repo_info(include_optional);

	cJSON* jsonschema_repo_info_1 = schema_repo_info_convertToJSON(schema_repo_info_1);
	printf("schema_repo_info :\n%s\n", cJSON_Print(jsonschema_repo_info_1));
	schema_repo_info_t* schema_repo_info_2 = schema_repo_info_parseFromJSON(jsonschema_repo_info_1);
	cJSON* jsonschema_repo_info_2 = schema_repo_info_convertToJSON(schema_repo_info_2);
	printf("repeating schema_repo_info:\n%s\n", cJSON_Print(jsonschema_repo_info_2));
}

int main() {
  test_schema_repo_info(1);
  test_schema_repo_info(0);

  printf("Hello world \n");
  return 0;
}

#endif // schema_repo_info_MAIN
#endif // schema_repo_info_TEST
