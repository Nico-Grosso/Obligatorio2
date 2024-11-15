#!/bin/bash

# Abre una terminal y ejecuta ./run_mininet.sh (con contraseña)
gnome-terminal -- bash -c "sshpass -p 'osboxes.org' ssh -t -X -p 2522 osboxes@127.0.0.1 'cd redes2024_ob1 && ./run_mininet.sh; exec bash'"

sleep 15

# Abre una terminal y ejecuta ./run_pox.sh
gnome-terminal -- bash -c "sshpass -p 'osboxes.org' ssh -t -X -p 2522 osboxes@127.0.0.1 'cd redes2024_ob1 && ./run_pox.sh; exec bash'"

sleep 5

# Abre una terminal y ejecuta ./run_sr.sh 127.0.0.1 vhost1
gnome-terminal -- bash -c "sshpass -p 'osboxes.org' ssh -t -X -p 2522 osboxes@127.0.0.1 'cd redes2024_ob1 && ./run_sr.sh 127.0.0.1 vhost1; exec bash'"

sleep 2

# Abre una terminal y ejecuta ./run_sr.sh 127.0.0.1 vhost2
gnome-terminal -- bash -c "sshpass -p 'osboxes.org' ssh -t -X -p 2522 osboxes@127.0.0.1 'cd redes2024_ob1 && ./run_sr.sh 127.0.0.1 vhost2; exec bash'"

sleep 2

# Abre una terminal y ejecuta ./run_sr.sh 127.0.0.1 vhost3
gnome-terminal -- bash -c "sshpass -p 'osboxes.org' ssh -t -X -p 2522 osboxes@127.0.0.1 'cd redes2024_ob1 && ./run_sr.sh 127.0.0.1 vhost3; exec bash'"

sleep 2

# Iniciar una nueva sesión SSH en la terminal actual
sshpass -p 'osboxes.org' ssh -t -X -p 2522 osboxes@127.0.0.1