# Build code in ubuntu

## pre-requisite

1. install vulkan ubuntu sdk / tar.gz

2. install the powershell in ubuntu

```shell


```

```shell
#/usr/bin/gcc and g++ already symbolinked to a version 13, in my case
# way 1
cmake -G Ninja -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ ..
# way 2
export CC=/usr/bin/gcc 
export CXX=/usr/bin/g++
cmake -G Ninja ..
# way 3
cd build
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake ..

###################################
# Prerequisites

/usr/bin/pwsh
# Update the list of packages
sudo apt-get update

# Install pre-requisite packages.
sudo apt-get install -y wget apt-transport-https software-properties-common

# Get the version of Ubuntu
source /etc/os-release

# Download the Microsoft repository keys
wget -q https://packages.microsoft.com/config/ubuntu/$VERSION_ID/packages-microsoft-prod.deb

# Register the Microsoft repository keys
sudo dpkg -i packages-microsoft-prod.deb

# Delete the Microsoft repository keys file
rm packages-microsoft-prod.deb

# Update the list of packages after we added packages.microsoft.com
sudo apt-get update

###################################
# Install PowerShell
sudo apt-get install -y powershell

# Start PowerShell
# pwsh

```

## wifi speed issu

sudo nano /etc/NetworkManager/conf.d/default-wifi-powersave-on.conf

Change wifi.powersave to 2 Press CTRL+X to exit, it'll ask if you want to save. Press Y and Enter.

```shell
#restart network adapter
sudo systemctl restart systemd-networkd 

Execute the following in the command line before executing the Git command:

export GIT_TRACE_PACKET=1
export GIT_TRACE=1
export GIT_CURL_VERBOSE=1

git config --global core.compression 0
#If you have good internet and are still getting this message, then it might be an issue with your post buffer. Use this command to increase it (for example) to 150 MiB:
git config --global http.postBuffer 157286400

git config --global pack.window 1

# What finally helped was this tip. Go to your user directory and edit .git/config and add:

git config --global core.packedGitLimit 512m
git config --global core.packedGitWindowSize 512m

git config --global pack.deltaCacheSize 2047m
git config --global pack.packSizeLimit 2047m
git config --global pack.windowMemory 2047m
# [core] 
#     packedGitLimit = 512m 
#     packedGitWindowSize = 512m 
# [pack] 
#     deltaCacheSize = 2047m 
#     packSizeLimit = 2047m 
#     windowMemory = 2047m


git config --global -e


git clone --depth 1 <repo_URI>
# cd to your newly created directory
git fetch --unshallow 
git pull --all
```

## sdl2 issue

sudo apt install libsdl2-dev
some dependenceis will cause audio issue

you will see extra steps in the cmake build as such: 

 38%] Building C object _deps/sdl2-build/CMakeFiles/SDL2-static.dir/src/SDL.c.o
[ 38%] Building C object _deps/sdl2-build/CMakeFiles/SDL2-static.dir/src/SDL_assert.c.o
[ 38%] Building C object _deps/sdl2-build/CMakeFiles/SDL2-static.dir/src/SDL_dataqueue.c.o
[ 38%] Building C object _deps/sdl2-build/CMakeFiles/SDL2-static.dir/src/SDL_error.c.o
[ 39%] Building C object _deps/sdl2-build/CMakeFiles/SDL2-static.dir/src/SDL_guid.c.o
[ 39%] Building C object _deps/sdl2-build/CMakeFiles/SDL2-static.dir/src/SDL_hints.c.o
[ 39%] Building C object _deps/sdl2-build/CMakeFiles/SDL2-static.dir/src/SDL_list.c.o
[ 39%] Building C object _deps/sdl2-build/CMakeFiles/SDL2-static.dir/src/SDL_log.c.o

## vma build issue
gcc 13

## gltf sdk issue
#include <limits> too a lot files

## Vulkan issue

  Could NOT find Vulkan (missing: Vulkan_LIBRARY Vulkan_INCLUDE_DIR
  SPIRV-Tools) (found version "")


  ```shell 
wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.3.296-noble.list https://packages.lunarg.com/vulkan/1.3.296/lunarg-vulkan-1.3.296-noble.list
sudo apt update
sudo apt install vulkan-sdk


# The following additional packages will be installed:
#   crashdiagnosticlayer dxc glslang-dev glslang-tools libglu1-mesa-dev
#   libjsoncpp25 libqt5concurrent5t64 libqt5opengl5-dev libqt5opengl5t64
#   libqt5printsupport5t64 libqt5sql5-sqlite libqt5sql5t64 libqt5test5t64
#   libqt5xml5t64 libvulkan-dev libvulkan1 libxext-dev libyaml-cpp0.8
#   lunarg-gfxreconstruct lunarg-via lunarg-vkconfig lunarg-vulkan-layers
#   qt5-qmake qt5-qmake-bin qtbase5-dev qtbase5-dev-tools qtchooser shaderc
#   slang spirv-cross spirv-cross-dev spirv-headers spirv-reflect spirv-tools
#   vma volk vulkan-extensionlayer vulkan-headers vulkan-profiles vulkan-tools
#   vulkan-utility-libraries vulkan-utility-libraries-dev
#   vulkan-validationlayers vulkancapsviewer
# Suggested packages:
#   libxext-doc default-libmysqlclient-dev firebird-dev libpq-dev libsqlite3-dev
#   unixodbc-dev
# The following NEW packages will be installed:
#   crashdiagnosticlayer dxc glslang-dev glslang-tools libglu1-mesa-dev
#   libjsoncpp25 libqt5concurrent5t64 libqt5opengl5-dev libqt5opengl5t64
#   libqt5printsupport5t64 libqt5sql5-sqlite libqt5sql5t64 libqt5test5t64
#   libqt5xml5t64 libvulkan-dev libxext-dev libyaml-cpp0.8 lunarg-gfxreconstruct
#   lunarg-via lunarg-vkconfig lunarg-vulkan-layers qt5-qmake qt5-qmake-bin
#   qtbase5-dev qtbase5-dev-tools qtchooser shaderc slang spirv-cross
#   spirv-cross-dev spirv-headers spirv-reflect spirv-tools vma volk
#   vulkan-extensionlayer vulkan-headers vulkan-profiles vulkan-sdk vulkan-tools
#   vulkan-utility-libraries vulkan-utility-libraries-dev
#   vulkan-validationlayers vulkancapsviewer
# The following packages will be upgraded:
#   libvulkan1

# Vulkan driver JSON manifest file, which is not modified by the SDK installer.
# cat /usr/share/vulkan/icd.d/nvidia_icd.json 
# {
#     "file_format_version" : "1.0.1",
#     "ICD": {
#         "library_path": "libGLX_nvidia.so.0",
#         "api_version" : "1.3.289"
#     }
# }

#uninstall
sudo apt purge vulkan-sdk
sudo apt autoremove


-- Found Vulkan: /usr/lib/x86_64-linux-gnu/libvulkan.so (found version "1.3.296") found components: SPIRV-Tools glslc glslangValidator
-- Vulkan_INCLUDE_DIR: /usr/include


  ```