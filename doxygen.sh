#!/bin/bash

# This script is used to generate the PDF documentation for the project.
# It requires the doxygen tool to be installed.
# html/index.html is the main page of the documentation.

# 1. 安装环境

# sudo apt-get install doxygen graphviz
for package in doxygen graphviz; do
    dpkg -l | grep $package
    if [ $? -eq 0 ]; then
        echo "$package is install"
    else
        echo "$package is not install"
        sudo apt install -y $package
    fi
done

# 2. 生成配置文件Doxyfile
if [ ! -f "Doxyfile" ]; then
    echo "Doxyfile not found"
else
    rm -rf Doxyfile
    echo "Doxyfile has been removed"
    if [ ! -d "build/doxygen" ]; then
        echo "dir build/doxygen not found"
    else
        echo "dir build/doxygen found"
        rm -rf build/doxygen
    fi
fi
doxygen -g

# 3. 打开Doxyfile，递归遍历当前目录的子目录，寻找被文档化的程序源文件，修改如下：
# 将RECURSIVE              = 替换为 RECURSIVE              = YES
sed -i 's/RECURSIVE.*= NO/RECURSIVE = YES/' Doxyfile

# 4. OUTPUT_DIRECTORY       = 替换为 OUTPUT_DIRECTORY       = build/doxygen
sed -i 's/OUTPUT_DIRECTORY.*=/OUTPUT_DIRECTORY = build\/doxygen/' Doxyfile

# 4. 文档生成
doxygen Doxyfile
