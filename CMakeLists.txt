cmake_minimum_required(VERSION 3.16)
project(most_rider LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CycloneDDS REQUIRED)

# Generates heartbeat.{c,h} from the frozen contract at build time — the
# generated sources are never committed (ADR-0001: contract-first).
idlc_generate(TARGET heartbeat_idl FILES interfaces/idl/heartbeat.idl)

function(add_zone_app name source)
  add_executable(${name} ${source})
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(${name} PRIVATE CycloneDDS::ddsc heartbeat_idl)
  target_compile_options(${name} PRIVATE -Wall -Wextra)
endfunction()

add_zone_app(front-zone apps/front-zone/src/main.cpp)
add_zone_app(central apps/central/src/main.cpp)
