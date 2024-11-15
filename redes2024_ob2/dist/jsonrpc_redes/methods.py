import random
import string

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

