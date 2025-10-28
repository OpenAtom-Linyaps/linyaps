# Unit Testing

Linglong uses gtest for unit testing, just pull test code and test data in test/ directory of project root.

Some tests are hard to run in CI environment, we disable them by default.

Here are some environment variables to control if tests run, all of them are empty by default.

| name              | value                            |
| ----------------- | -------------------------------- |
| LINGLONG_TEST_ALL | If set, will run all unit tests. |
