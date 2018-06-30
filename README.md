# MTreeServer

This server application is part of my Thesis task. It is developed on Ubuntu Client with Visual Studio Code IDE. (https://code.visualstudio.com/)
Used VSCode extension: Microsoft C/C++ for Visual Studio Code

Used C++ compiler: gcc
If not presents:
sudo apt install gcc (C++ compiler)
sudo apt install build-essential (basic C++ libraries, it contains gcc)

Dependencies: curl, jansson, pthread
curl: Download current Source Archives curl-*.tar.gz.
Extract, follow (curl_lib_location)/docs/INSTALL.md instructions. Most likely the following:
./configure
make
sudo make install

jansson: git clone https://github.com/akheron/jansson
autoreconf -i (it can fail with some undefined macros, it can be solved with installing the missing tools with apt install)
./configure
make
sudo make install

pthread: it should already installed on Ubuntu.