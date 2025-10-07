Steps for How to configure the C based server to run on windows

In this server I am accepting the request from browser for webpage and sensor data request. Server also receiving the sensor data in csv format.

I am using the ubentu 22.04.5 LTS terminal (WSL) to run my server inside the windows machine.

Is WSL creates its own virtual IP, it is not accessed to the outside devices. So we need some way to connect server with the outside devices.
Here we use the port forwarding technique. We configure the windows ip to forward the all messages it receive on any of its network interface and specific port to ip and port of the server 
we also allowed all the TCP connections on port 10013 through the windows wirewall.

STEPS:

1) Download the arduino sketch of ESP8266. before downloading the sketch add SSID and wi-fi password. 
   Also add the IP and port number of server. (If using the windows machine then add the IP of the windows and not the WSL)

2) Check the ip of the WSL using the 'ip addr show' command. Windows will forward the connection it receives on the specified port to this IP.

3) Open the powershell as admin for port forwarding and wirewall enable

Port forwarding command - netsh interface portproxy add v4tov4 listenport=port_number listenaddress=0.0.0.0 connectport=port_number connectaddress=virtual_ip
Ex - netsh interface portproxy add v4tov4 listenport=10013 listenaddress=0.0.0.0 connectport=10013 connectaddress=172.26.238.193

command to enable the wirewall - netsh advfirewall firewall add rule name="WSL TCPport_number" dir=in action=allow protocol=TCP localport=port_number
Ex - netsh advfirewall firewall add rule name="WSL TCP10013" dir=in action=allow protocol=TCP localport=10013

4) Run the server in terminal and in browser enter 'localhost:10013' your browser will receive the webpage.