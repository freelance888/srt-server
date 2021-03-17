# srt-server

## Dependencies
  1. SRT (https://github.com/Haivision/srt)
  2. cmake > 3.16

## Installation
### Build from source

srt-server can only build under linux. Was tested under Ubuntu 20.04.

1. Build SRT:
```
git clone https://github.com/Haivision/srt.git
sudo apt-get update
sudo apt-get install -y tclsh pkg-config cmake libssl-dev build-essential
cd srt 
./configure --prefix=/usr
make
sudo make install
```
2. Build srt-server:
```
git clone https://github.com/ALLATRA-IT/srt-server.git
cd srt-server
cmake .
make
sudo make install
```

Oneliner installer:
```
sudo apt update && sudo apt install -y tclsh pkg-config cmake libssl-dev build-essential git && git clone https://github.com/Haivision/srt.git && cd srt && ./configure --prefix=/usr && make && sudo make install && cd .. && git clone https://github.com/ALLATRA-IT/srt-server.git && cd srt-server && cmake . && make && sudo make install
```

### Download release binaries
https://github.com/ALLATRA-IT/srt-server/releases

## Usage
srt-server [PORT_RECEIVE PORT_SEND [PORT_RTMP]]

*if PORT_RECEIVE and PORT_SEND are not specified, by default server receives on port 9000 and sends on 9001.* 

*if **PORT_RTMP** is specified, rtmp stream is received on rtmp://0.0.0.0:PORT_RTMP/rtmp/rtmp2srt*

***ARGUMENTS ARE POSITION SENSITIVE***