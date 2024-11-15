from socket import *
import json
from .funciones import *
import uuid

'''Definición de una excepción personalizada para errores en el protocolo JSON-RPC'''
class JSONRPCError(Exception):
    def __init__(self, code, message): # Inicializa la excepción con un código y un mensaje de error
        super().__init__(f"Error {code}: {message}")
        self.code = code
        self.message = message

class Cliente:
    '''Inicializa el cliente con la dirección y el puerto del servidor'''
    def __init__(self, address, port):
        self.port = port
        self.address = address

    '''Método que se llama cuando se intenta acceder a un atributo no existente en el objeto, creamos métodos de forma dinámica'''
    def __getattr__(self, name):
        def method(*args, notify = False, **kwargs):
            # Crea un nuevo socket y se conecta al servidor
            self.socket = socket(AF_INET, SOCK_STREAM)
            self.socket.connect((self.address,self.port))
            
            if args and kwargs: # Verifica si se han pasado argumentos posicionales y de palabra clave al mismo tiempo
                self.socket.close()
                raise ValueError('JSON spec allows positional arguments OR keyword arguments, not both.')

            params = args if args else kwargs # Usa args si están presentes, de lo contrario usa kwargs

            if not notify: # Si no es una notificación lo que se desea enviar
                request = {
                    "jsonrpc" : "2.0",
                    "method" : name,
                    "params" : params,
                    "id" : str(uuid.uuid4()) # Genera un identificador único para la solicitud
                }
            else: # En caso de ser una notificación, arma la solicitud de este modo
                request = {
                    "jsonrpc" : "2.0",
                    "method" : name,
                    "params" : params
                }
            message = json.dumps(request) # Convierte el request a formato JSON
            
            try:
                self.socket.sendall(message.encode('utf-8')) # Envía el mensaje al servidor
            except Exception as e:
                self.socket.close()
                print("Conexión Cerrada")
                return

            print(f'CLIENT | REQUEST : {message}')

            if not notify: # Si el cliente no manda una notificación, espera respuesta
                response = recvall(self.socket) # Recibe la respuesta del servidor
                print(f'CLIENT | RESPONSE : {response}') # Imprime la respuesta recibida

                try:
                    response_data = json.loads(response) # Intenta parsear la respuesta como JSON
                except json.JSONDecodeError as e:
                    self.socket.close()
                    raise Exception("Failed to decode response as JSON") from e

                if 'error' in response_data: # Verifica si hay un error en la respuesta
                    error_info = response_data['error']
                    code = error_info.get('code', 'Unknown code') # Obtiene el código de error
                    message = error_info.get('message', 'Unknown error') # Obtiene el mensaje de error
                    self.socket.close()
                    raise JSONRPCError(code,message)
                
                self.socket.close()
                return response_data.get('result') # Retorna el resultado de la respuesta
            
        return method

def connect(address, port):
    return Cliente(address, port)




