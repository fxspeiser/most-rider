cmake_minimum_required(VERSION 3.16)
project(most_rider LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CycloneDDS REQUIRED)

# Generates {heartbeat,discovery,vehicle}.{c,h} from the frozen contracts at
# build time — the generated sources are never committed (ADR-0001:
# contract-first).
idlc_generate(TARGET heartbeat_idl FILES interfaces/idl/heartbeat.idl)
idlc_generate(TARGET discovery_idl FILES interfaces/idl/discovery.idl)
idlc_generate(TARGET vehicle_idl FILES interfaces/idl/vehicle.idl)
idlc_generate(TARGET sensor_burst_idl FILES interfaces/idl/sensor_burst.idl)

function(add_zone_app name source)
  add_executable(${name} ${source})
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(${name} PRIVATE CycloneDDS::ddsc heartbeat_idl discovery_idl vehicle_idl sensor_burst_idl)
  target_compile_options(${name} PRIVATE -Wall -Wextra)
endfunction()

add_zone_app(zone-runtime apps/zone-runtime/src/main.cpp)
add_zone_app(central apps/central/src/main.cpp)
add_zone_app(energy-service apps/energy-service/src/main.cpp)
add_zone_app(body-service apps/body-service/src/main.cpp)
add_zone_app(load-generator apps/load-generator/src/main.cpp)
add_zone_app(propulsion-monitor apps/propulsion-monitor/src/main.cpp)
add_zone_app(telemetry-bridge apps/telemetry-bridge/src/main.cpp)
