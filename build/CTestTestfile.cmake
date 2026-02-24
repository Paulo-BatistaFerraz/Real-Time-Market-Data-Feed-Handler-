# CMake generated Testfile for 
# Source directory: D:/market-data-feed-handler
# Build directory: D:/market-data-feed-handler/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(AllTests "D:/market-data-feed-handler/build/run_tests.exe")
set_tests_properties(AllTests PROPERTIES  _BACKTRACE_TRIPLES "D:/market-data-feed-handler/CMakeLists.txt;66;add_test;D:/market-data-feed-handler/CMakeLists.txt;0;")
subdirs("_deps/nlohmann_json-build")
subdirs("_deps/googletest-build")
