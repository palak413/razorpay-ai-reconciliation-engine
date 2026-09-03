cmake_minimum_required(VERSION 3.12)
project(RazorpayReconciliationEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)

include_directories(src ${SQLITE3_INCLUDE_DIRS})

add_executable(reconciliation_engine
    src/main.cpp
    src/database.cpp
    src/reconciliation_engine.cpp
)

target_link_libraries(reconciliation_engine PRIVATE SQLite3::SQLite3)