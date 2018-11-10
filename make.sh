#!/bin/sh
# if [ -f "apps/sdk.cbp" ]; then
# /usr/bin/CodeBlocks/codeblocks.exe /na /nd --no-splash-screen --build sdk.cbp --target=Release
rm bin/Debug/flash_image.exe
rm ./flash_image.exe
E:/codeblocks17_6081/codeblocks17/"codeblocks 17.12"/codeblocks.exe /na /nd --no-splash-screen --build flash_image.cbp --target=Debug
cp bin/Debug/flash_image.exe ./
./flash_image.exe -infile res/fat.res 
# ssh-keygen -t rsa -C "1043623557@qq.com"
# pissh -T git@github.com
# fi

