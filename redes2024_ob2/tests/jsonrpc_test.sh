sh echo "=============================================="
sh echo "Capturando tráfico en client, vhosts y servers"
sh echo "=============================================="
client tcpdump -i any -w capturas/jsonrpc_test_client.pcap > /dev/null 2>&1 &
vhost1 tcpdump -i any -w capturas/jsonrpc_test_vhost1.pcap > /dev/null 2>&1 &
vhost2 tcpdump -i any -w capturas/jsonrpc_test_vhost2.pcap > /dev/null 2>&1 &
vhost3 tcpdump -i any -w capturas/jsonrpc_test_vhost3.pcap > /dev/null 2>&1 &
server1 tcpdump -i any -w ../capturas/jsonrpc_test_server1.pcap > /dev/null 2>&1 &
server2 tcpdump -i any -w ../capturas/jsonrpc_test_server2.pcap > /dev/null 2>&1 &
sh sleep 2

sh echo "=========================================="
sh echo "Ejecutando test-server.py y test-client.py"
sh echo "=========================================="
server1 python3.8 test-server.py &
server2 python3.8 test-server.py &
sh sleep 3
client python3.8 dist/test-client.py

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
sh echo "Capturas guardadas en los archivos jsonrpc_test_*.pcap"
sh echo "======================================================"