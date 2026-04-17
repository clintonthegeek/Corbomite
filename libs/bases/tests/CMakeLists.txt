cmake_minimum_required(VERSION 3.19)
project(Corbomite_BasesTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Per-test executables are appended as each test lands during Cluster K
# phase execution. Start empty; Task 1.2 lands the first test.
