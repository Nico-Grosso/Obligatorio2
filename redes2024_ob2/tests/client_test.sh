sh echo "=============================================="
sh echo "Capturando tráfico en client, vhosts y servers"
sh echo "=============================================="
client tcpdump -i any -w capturas/client_test_client.pcap > /dev/null 2>&1 &
vhost1 tcpdump -i any -w capturas/client_test_vhost1.pcap > /dev/null 2>&1 &
vhost2 tcpdump -i any -w capturas/client_test_vhost2.pcap > /dev/null 2>&1 &
vhost3 tcpdump -i any -w capturas/client_test_vhost3.pcap > /dev/null 2>&1 &
server1 tcpdump -i any -w ../capturas/client_test_server1.pcap > /dev/null 2>&1 &
server2 tcpdump -i any -w ../capturas/client_test_server2.pcap > /dev/null 2>&1 &
sh sleep 2

sh echo "================================="
sh echo "Ping desde CLIENTE hacia SERVER 1"
sh echo "================================="
sh sleep 1
client ping -c 3 200.0.0.10

sh echo "================================="
sh echo "Ping desde CLIENTE hacia SERVER 2"
sh echo "================================="
sh sleep 1
client ping -c 3 200.100.0.15

sh echo "============================================"
sh echo "Ping desde CLIENTE hacia VHOST1 (100.0.0.50)"
sh echo "============================================"
sh sleep 1
client ping -c 3 100.0.0.50

sh echo "=========================================="
sh echo "Ping desde CLIENTE hacia VHOST2 (10.0.1.1)"
sh echo "=========================================="
sh sleep 1
client ping -c 3 10.0.1.1

sh echo "=========================================="
sh echo "Ping desde CLIENTE hacia VHOST3 (10.0.2.2)"
sh echo "=========================================="
sh sleep 1
client ping -c 3 10.0.2.2

sh echo "============================================="
sh echo "Ping desde CLIENTE hacia interfaz inexistente"
sh echo "============================================="
sh sleep 1
client ping -c 3 200.0.0.51

sh echo "======================================================="
sh echo "Ping desde CLIENTE hacia interfaz de subred inexistente"
sh echo "======================================================="
sh sleep 1
client ping -c 3 50.0.0.1

sh echo "===================================================="
sh echo "Traceroute desde CLIENTE hacia SERVER 1 (200.0.0.10)"
sh echo "===================================================="
sh sleep 1
client traceroute -n -I 200.0.0.10

sh echo "======================================================"
sh echo "Traceroute desde CLIENTE hacia SERVER 2 (200.100.0.15)"
sh echo "======================================================"
sh sleep 1
client traceroute -n -I 200.100.0.15

sh echo "================================================="
sh echo "Traceroute desde CLIENTE hacia VHOST1 (100.0.0.50)"
sh echo "================================================="
sh sleep 1
client traceroute -n -I 100.0.0.50

sh echo "================================================"
sh echo "Traceroute desde CLIENTE hacia VHOST2 (10.0.0.2)"
sh echo "================================================"
sh sleep 1
client traceroute -n -I 10.0.0.2

sh echo "===================================================="
sh echo "Traceroute desde CLIENTE hacia VHOST3 (200.100.0.50)"
sh echo "===================================================="
sh sleep 1
client traceroute -n -I 200.100.0.50

client pkill tcpdump
vhost1 pkill tcpdump
vhost2 pkill tcpdump
vhost3 pkill tcpdump
server1 pkill tcpdump
server2 pkill tcpdump

sh echo "====================================================="
sh echo "Capturas guardadas en los archivos client_test_*.pcap"
sh echo "====================================================="