# Installation

## Build

```bash
mkdir -v build
cd build
cmake ..
make -j8
sudo make install
```

## Test && Run

```bash
# install repo
> curl https://sh.linglong.space | sh

# install an app, just in deepin
> ll-cli install com.163.music

# run app with sandbox
> ll-cli run com.163.music

# show running container
> ll-cli ps
bc000426d8884c7fbe804996f24144ce 21751 @ /run/user/1000/linglong/bc000426d8884c7fbe804996f24144ce

# kill container
> ll-cli kill bc00
```
