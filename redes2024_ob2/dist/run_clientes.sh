#!/bin/bash

# Abre la terminal para el servidor
gnome-terminal -- bash -c "python3.8 ./test-server.py; exec bash"

gnome-terminal -- bash -c "sleep 3; python3.8 ./test-client.py; exec bash"

gnome-terminal -- bash -c "sleep 2; python3.8 ./test-client.py; exec bash"

# Abre las terminales para los clientes
for i in {1..9}; do
    gnome-terminal -- bash -c "python3.8 ./test-client.py; exec bash"
done