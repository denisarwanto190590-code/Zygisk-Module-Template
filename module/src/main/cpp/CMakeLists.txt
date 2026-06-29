cmake_minimum_required(VERSION 3.10.2)
project("zygisk_esp")

add_library(zygisk_esp SHARED main.cpp)

# Menghubungkan ke pustaka bawaan Android untuk kebutuhan log dan grafis
target_link_libraries(zygisk_esp log android)
