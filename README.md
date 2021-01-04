<h1>srt-server</h1>

<h3>Dependencies</h3>
 - SRT (https://github.com/Haivision/srt)
 - cmake > 3.17.2

<h3>Build</h3>
srt-server can only build under linux. Was tested under Ubuntu 20.04.

 - git clone https://github.com/AKM2109/srt-server.git
 - cmake .
 - make
 - sudo make install
 
<h3>Usage</h3>
 <b>srt_server [PORT_RECEIVE PORT_SEND]</b>

if PORT_RECEIVE and PORT_SEND are not specified, by default server receives data on 9000 port and sends data from 9001 port
