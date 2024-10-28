sh echo "================================="
sh echo "Ping desde CLIENTE hacia SERVER 1"
sh echo "================================="
client ping -c 3 200.0.0.10

sh echo "================================="
sh echo "Ping desde CLIENTE hacia SERVER 2"
sh echo "================================="
client ping -c 3 200.100.0.15

sh echo "================================="
sh echo "Ping desde CLIENTE hacia VHOST1 (100.0.0.50)"
sh echo "================================="
client ping -c 3 100.0.0.50

sh echo "================================="
sh echo "Ping desde CLIENTE hacia VHOST2 (10.0.1.1)"
sh echo "================================="
client ping -c 3 10.0.1.1

sh echo "================================="
sh echo "Ping desde CLIENTE hacia VHOST3 (10.0.2.2)"
sh echo "================================="
client ping -c 3 10.0.2.2

sh echo "================================="
sh echo "Ping desde CLIENTE hacia interfaz inexistente"
sh echo "================================="
client ping -c 3 200.0.0.51

sh echo "================================="
sh echo "Ping desde CLIENTE hacia interfaz de subred inexistente"
sh echo "================================="
client ping -c 3 50.0.0.1

sh echo "================================="
sh echo "Traceroute desde CLIENTE hacia SERVER 1 (200.0.0.10)"
sh echo "================================="
client traceroute -n -I 200.0.0.10

sh echo "================================="
sh echo "Traceroute desde CLIENTE hacia SERVER 2 (200.100.0.15)"
sh echo "================================="
client traceroute -n -I 200.100.0.15

sh echo "================================="
sh echo "Traceroute desde CLIENTE hacia VHOST1 (100.0.0.50)"
sh echo "================================="
client traceroute -n -I 100.0.0.50

sh echo "================================="
sh echo "Traceroute desde CLIENTE hacia VHOST2 (10.0.0.2)"
sh echo "================================="
client traceroute -n -I 10.0.0.2

sh echo "================================="
sh echo "Traceroute desde CLIENTE hacia VHOST3 (200.100.0.50)"
sh echo "================================="
client traceroute -n -I 200.100.0.50
