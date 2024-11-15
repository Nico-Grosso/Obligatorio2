from jsonrpc_redes import Server
import string
import random
import threading
import time
import sys

def test_server():
    host1, port1 = '', 8080

    #MÉTODOS SERVER 1
    def add(*params):
        return sum(params)

    def generate_email_user(user_name,server):
        email = user_name + "@" + server + ".com"
        return email

    def generate_password(comienzo, largo = 8):
        characters = string.ascii_letters + string.digits + string.punctuation
        password = ''.join(random.choice(characters) for _ in range(largo))
        return comienzo + password
    
    def multiplos_de_n(list, n):
        res = []
        for num in list:
            if (num % n == 0):
                res.append(num)
        return res
    
    #MÉTODOS SERVER 2
    def is_sum_even(a,b):
        return (a+b) % 2 == 0

    def get_ocurrences(cadena, caracter):
        return cadena.count(caracter)

    def multiplicar(*params):
        result = 1
        for p in params:
            result = result*p
        return result
    
    server1 = Server(host1,port1)
    server1.add_method(add)
    server1.add_method(generate_email_user)
    server1.add_method(generate_password)
    server1.add_method(is_sum_even)
    server1.add_method(get_ocurrences)
    server1.add_method(multiplicar)
    server1.add_method(multiplos_de_n)
    server_thread = threading.Thread(target=server1.serve)
    server_thread.daemon = True
    server_thread.start()
    
    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        print('Terminado.')
        sys.exit()

if __name__ == "__main__":
    test_server()



    