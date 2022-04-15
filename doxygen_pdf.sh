#!/bin/bash

# This script is used to generate the PDF documentation for the project.
# It requires the doxygen tool to be installed.

# 1. 安装环境

# sudo apt-get install doxygen graphviz texlive-full latex-cjk-chinese* cjk-latex
for package in doxygen graphviz texlive-full latex-cjk-chinese* cjk-latex
do
    dpkg -l | grep $package
    if [ $? -eq 0 ];
    then
    echo "$package is install"
    else
    echo "$package is not install"
    sudo apt install -y $package
    fi
done

# 2. 生成配置文件Doxyfile
if [ ! -f "Doxyfile" ];then
    echo "Doxyfile not found"
else
    rm -rf Doxyfile
echo "Doxyfile has been removed"
    if [! -d "latex" ];then
    echo "dir latex not found"
    else
    echo "dir latex found"
    rm -rf latex
    fi

    if [! -d "html" ];then
    echo "dir html not found"
    else
    echo "dir html found"
    rm -rf latex
    fi

    if [ ! -f $(basename "$(dirname "$PWD")")_api_doc.pdf ];then
    echo "api_doc.pdf not found"
    else
    rm -rf $(basename "$(dirname "$PWD")")_api_doc.pdf
    echo "api_doc.pdf has been removed"
    fi
fi
doxygen -g

# 3. 打开Doxyfile，递归遍历当前目录的子目录，寻找被文档化的程序源文件，修改如下：
# 将RECURSIVE              = 替换为 RECURSIVE              = YES
sed -i 's/RECURSIVE.*= NO/RECURSIVE = YES/' Doxyfile

# 4. 文档生成
doxygen Doxyfile

# 5.进入到latex目录

cd latex

# 6. 修改前面doxygen Doxyfile生成的latex文件：refman.tex
#    将其中的:

# ```plain
# \begin{document}
# ```

# 改为：

# ```plain
# \usepackage{CJKutf8}
# \begin{document}
# \begin{CJK}{UTF8}{gbsn}
# ```

sed -i 's/\\begin{document}/\\usepackage{CJKutf8}\n\\begin{document}\n\\begin{CJK}{UTF8}{gbsn}/' refman.tex

# 并将其中的:

# ```plain
# \end{document}
# ```

# 改为：

# ```plain
# \end{CJK}
# \end{document}
# ```
sed -i 's/\\end{document}/\\end{CJK}\n\\end{document}/' refman.tex

# 8. 生成pdf文件
make -j
echo $(basename "$(dirname "$PWD")")
mv refman.pdf ../$(basename "$(dirname "$PWD")")_api_doc.pdf