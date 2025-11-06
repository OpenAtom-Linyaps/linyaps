# Unit Testing

Linyaps uses GoogleTest (gtest) for unit testing. Simply include test code and test data in the `test/` directory of the project root.

Since some tests are difficult to run in CI environments, they are disabled by default.

Here are some environment variables to control test execution. All of them are empty by default.

| Variable Name     | Value                           | Description                     |
| ----------------- | ------------------------------- | ------------------------------- |
| LINGLONG_TEST_ALL | If set, will run all unit tests | Enables all unit test execution |
