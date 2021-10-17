<h1>TCP Proxy</h1>

<h2>How to build:</h2>
You will need Linux / Unix to build this code, because this repo include Boost Asio for unix. This source code was developed using Visual Studio Code in Linux kubuntu. You should be able to compile using any other Linux Distro.

Please follow the steps bellow :

1.  install Visual Studio Code + Install extension C/C++ IntelliSense. If you haven't install GCC, please follow this link to install GCC at Linux : https://code.visualstudio.com/docs/cpp/config-linux
2.  install : sudo apt-get install libboost-all-dev
3.  install : sudo apt install redis
4.  Open Visual Studio Code and then File > Open Folder > Select "tcp_proxy" that you have clone.
5.  Still on VScode, Open "main.cpp" then click tab Terminal > Run Build Task.

<h2>How to Run the Proxy :</h2>
./main  <local host ip> <local port> <forward host ip> <forward port>
For example we will connect to a simple HTTP server from 24invitation.com:8080. To get the server IP address, you can try to ping the domain.IP of 24invitation.com is 185.201.8.86. We will try connect to a simple HTTP server at port 8080.
    
    eg : ./main 0.0.0.0 1234 185.201.8.86 8080
    
<h2>Data Logger</h2>
Any Incoming and Outgoing on each IP is logged into file incoming.csv and outgoing.csv. Just in case if the data log will be used for further analysis. The data log format is shown below:

    Date Time;Client IP Address:Client Port;Data Length in Bytes;"Raw Data"\n

Each incoming data from Client is also counted using Redis, Therefore you must install Redis to your system. The keyvalue is Client IP Address.

*   To get available Client IPs, please follow command bellow using your terminal:

        $ redis-cli keys "*"

*   To retrive how many bytes received from specific Client IP address, please follow command bellow using your terminal:

        $ redis-cli GET <IP Address>
        eg : $ redis-cli GET 127.0.0.1

