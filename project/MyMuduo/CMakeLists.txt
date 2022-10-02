cmake_minimum_required(VERSION 2.5)
project(mymuduo)

#mymuduo最终编译成为动态库，这里要设置动态库的路径
#PROJCET_SOURCE_DIR 表示当前源码的根目录，为camke预置变量
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#-g 是为了告诉编译器产生调试信息，方便后续调试 同时-std=c++11启动了c++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

#定义参与编译的源代码文件，用一个变量表示
aux_source_directory(. SRC_LIST)

#编译生成动态库mymuduo
add_library(mymuduo SHARED ${SRC_LIST}) 