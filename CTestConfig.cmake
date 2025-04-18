## This file should be placed in the root directory of your project.
## Then modify the CMakeLists.txt file in the root directory of your
## project to incorporate the testing dashboard.
## # The following are required to uses Dart and the Cdash dashboard
##   ENABLE_TESTING()
##   INCLUDE(CTest)
set(CTEST_PROJECT_NAME "srsRAN")
set(CTEST_NIGHTLY_START_TIME "00:00:00 GMT")

set(CTEST_DROP_METHOD "http")
set(CTEST_DROP_SITE "my.cdash.org")
set(CTEST_DROP_LOCATION "/submit.php?project=srsRAN")
set(CTEST_DROP_SITE_CDASH TRUE)
set(VALGRIND_COMMAND_OPTIONS "--error-exitcode=1 --trace-children=yes --leak-check=full --show-reachable=yes --vex-guest-max-insns=25")