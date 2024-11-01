sh echo "=============================================="
sh echo "Capturando tráfico en client, vhosts y servers"
sh echo "=============================================="
client tcpdump -i any -w servers_test_client.pcap > /dev/null 2>&1 &
vhost1 tcpdump -i any -w servers_test_vhost1.pcap > /dev/null 2>&1 &
vhost2 tcpdump -i any -w servers_test_vhost2.pcap > /dev/null 2>&1 &
vhost3 tcpdump -i any -w servers_test_vhost3.pcap > /dev/null 2>&1 &
server1 tcpdump -i any -w client_test_server1.pcap > /dev/null 2>&1 &
server2 tcpdump -i any -w client_test_server2.pcap > /dev/null 2>&1 &
sh sleep 2

sh echo "================================="
sh echo "Ping desde SERVER1 hacia CLIENTE"
sh echo "================================="
sh sleep 2
server1 ping -c 3 client

sh echo "================================="
sh echo "Ping desde SERVER2 hacia CLIENTE"
sh echo "================================="
sh sleep 2
server2 ping -c 3 client

sh echo "======================================"
sh echo "Traceroute desde SERVER1 hacia CLIENTE"
sh echo "======================================"
sh sleep 2
server1 traceroute -n -I client

sh echo "======================================"
sh echo "Traceroute desde SERVER2 hacia CLIENTE"
sh echo "======================================"
sh sleep 2
server2 traceroute -n -I client

sh echo "================================"
sh echo "Finalizando capturas de tráfico"
sh echo "================================"
sh sleep 2
client pkill tcpdump
vhost1 pkill tcpdump
vhost2 pkill tcpdump
vhost3 pkill tcpdump
server1 pkill tcpdump
server2 pkill tcpdump

sh echo "======================================================"
sh echo "Capturas guardadas en los archivos servers_test_*.pcap"
sh echo "======================================================"
