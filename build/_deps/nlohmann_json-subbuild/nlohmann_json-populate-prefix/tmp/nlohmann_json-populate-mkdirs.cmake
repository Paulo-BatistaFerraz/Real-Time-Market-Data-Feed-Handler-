# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/market-data-feed-handler/build/_deps/nlohmann_json-src"
  "D:/market-data-feed-handler/build/_deps/nlohmann_json-build"
  "D:/market-data-feed-handler/build/_deps/nlohmann_json-subbuild/nlohmann_json-populate-prefix"
  "D:/market-data-feed-handler/build/_deps/nlohmann_json-subbuild/nlohmann_json-populate-prefix/tmp"
  "D:/market-data-feed-handler/build/_deps/nlohmann_json-subbuild/nlohmann_json-populate-prefix/src/nlohmann_json-populate-stamp"
  "D:/market-data-feed-handler/build/_deps/nlohmann_json-subbuild/nlohmann_json-populate-prefix/src"
  "D:/market-data-feed-handler/build/_deps/nlohmann_json-subbuild/nlohmann_json-populate-prefix/src/nlohmann_json-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/market-data-feed-handler/build/_deps/nlohmann_json-subbuild/nlohmann_json-populate-prefix/src/nlohmann_json-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/market-data-feed-handler/build/_deps/nlohmann_json-subbuild/nlohmann_json-populate-prefix/src/nlohmann_json-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
