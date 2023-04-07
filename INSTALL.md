# Installation

## Build

```bash
mkdir -v build
cd build
cmake ..
cmake --build .
sudo cmake --install .
```

## Build For Developers

```bash
mkdir -v build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_DEBUG_SANIT=address ..
cmake --build .
sudo cmake --install .
```

DCMAKE_DEBUG_SANIT = 'address' open asan for debug builds
DCMAKE_DEBUG_SANIT = 'thread' open tasn for debug builds

## Run

```bash
# install repo
> curl https://sh.linglong.dev | sh

# install an app, just in deepin
> ll-cli install com.163.music

# run app with sandbox
> ll-cli run com.163.music

# show running container
> ll-cli ps
App                                             ContainerID                         Pid     Path
bc000426d8884c7fbe804996f24144ce 21751 @ /run/user/1000/linglong/bc000426d8884c7fbe804996f24144ce

# kill container
> ll-cli kill bc00
```

## Coverage Test

Add --coverage in CMakeLists.txt

```bash
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
```

Use gcov, lcov to generate convert HTML report, make sure at the top level of the project.

```bash
./scripts/code_coverage.sh
```
