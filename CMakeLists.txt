cmake_minimum_required(VERSION 3.20)

project(ChordRecognition)

set(CMAKE_CXX_STANDARD 20)

find_package(SndFile REQUIRED)

add_executable(chord_recognition
    src/main.cpp
)

target_link_libraries(chord_recognition
    PRIVATE
    SndFile::sndfile
)