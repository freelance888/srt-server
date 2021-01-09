

<h1>srt-server</h1>

<h3>Dependencies</h3>
<ul>
  <li>SRT (https://github.com/Haivision/srt)</li>
  <li>cmake > 3.17.2</li>
</ul>

<h3>Build</h3>
<div style="padding-left: 20px">
  <h4>1. Build from source</h4>
  srt-server can only build under linux. Was tested under Ubuntu 20.04.
  
  ```
  Build SRT:
  - git clone https://github.com/Haivision/srt.git
  - sudo apt-get update
  - sudo apt-get install -y tclsh pkg-config cmake libssl-dev build-essential
  - ./configure --prefix=/usr
  - make
  - sudo make install
  
  Build srt-server:
  - git clone https://github.com/AKM2109/srt-server.git
  - cmake .
  - make
  - sudo make install
  ```

  <h4>2. Download release binaries</h4>

</div>

<h3>Usage</h3>
<div style="padding-left: 20px">
<h4>srt-server [PORT_RECEIVE PORT_SEND]</h4>

_if PORT_RECEIVE and PORT_SEND are not specified, by default server receives on port 9000 and sends on 9001_
</div>