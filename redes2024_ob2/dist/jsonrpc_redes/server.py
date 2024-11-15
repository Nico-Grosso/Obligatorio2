from socket import *
import json
import inspect
from threading import Thread
from .methods import *
from .funciones import *

class Server:
    '''Inicializa el servidor con la dirección y el puerto definidos'''
    def __init__(self, address, port):
        self.port = port
        self.address = address
        self.methods = {}
        self.variable_methods = {}

    '''Crea un socket TCP y empieza a escuchar conexiones'''
    def serve(self):
        s = socket(AF_INET, SOCK_STREAM)
        s.bind((self.address, self.port)) # Asocia el socket
        s.listen() # Escuchando...
        print(f"Serving on {self.address}:{self.port}")
            
        while True:
            conn, addr = s.accept() # Acepta una nueva conexión
            client_thread = Thread(target=self.handle_client, args=(conn,)) # Crea un hilo con el cliente y llama al manejador
            client_thread.start() # Lo comienza

    '''Función para agregar métodos según corresponda el tipo'''
    def add_method(self, func):
        if self.is_variable_args(func):
            self.variable_methods[func.__name__] = func
        self.methods[func.__name__] = func
        
    '''Función para verificar si un método tiene argumentos variables'''
    def is_variable_args(self, func):
        sig = inspect.signature(func)
        
        for param in sig.parameters.values():
            if param.kind == inspect.Parameter.VAR_POSITIONAL or param.kind == inspect.Parameter.VAR_KEYWORD:
                return True
        return False # Si no hay argumentos variables retorna False

    '''Manejador de conexión del cliente'''
    def handle_client(self, skt):

        try:
            while True:
                message = recvall(skt)  # Recive mensaje en formato json

                if not message:  # Break the loop si ningún mensaje es recevido (cliente desconectado)
                    break
                
                print(f'SERVER | REQUEST : {message}')

                request = self.parse_message(message) # Parsea el mensaje JSON

                notification = not 'id' in request # Verifica si es una notificación

                if request is None:             # Si el mensaje recibido no se pudo parsear
                    if not notification:        # y no fue una notificación
                        self.send_error_response(skt, -32700, "Parse error", None) # Responde
                    continue

                # Validación de la estructura y contenido del mensaje
                validation_error = self.validate_request(request)
                if validation_error:            # Si hay error de validación de la solicitud
                    if not notification:        # y no fue una notificación
                        self.send_error_response(skt, validation_error["code"], validation_error["message"], request.get('id')) # Responde
                    continue

                # Procesa la solicitud
                response = self.process_request(request)
                if not notification:  # Si no fue una notificación
                    skt.sendall(json.dumps(response).encode('utf-8'))   # Responde
                    print(f'SERVER | RESPONSE : {json.dumps(response)}')

        except Exception as e: # Si ocurre algún error interno, responde
            self.send_error_response(skt, -32603, "Internal error", None, str(e))
        skt.close()
        
    '''Función para parsear el mensaje JSON'''
    def parse_message(self, message):
        try:
            return json.loads(message)
        except json.JSONDecodeError:
            return None

    '''Función que valida la solicitud recibida y verifica su conformidad con el estánder JSON-RPC'''
    def validate_request(self, request):
        if 'jsonrpc' not in request or request['jsonrpc'] != "2.0":
            return {"code": -32600, "message": "Invalid Request"}
        if 'method' not in request or not isinstance(request['method'],str):
            return {"code": -32600, "message": "Invalid Request"}
        if 'params' in request and not isinstance(request['params'], (list, dict)):
            return {"code": -32602, "message": "Invalid params"}
        return None

    '''Función para procesar la solicitud y ejecutar el método correspondiente'''
    def process_request(self, request):
        nombre_metodo = request['method']
        parametros = request.get('params', [])
        if 'id' in request: # Distingue si hay id en la solicitud para responder
            identificador = request['id'] 

            metodo = self.methods.get(nombre_metodo)
            if metodo:
                return self.invoke_method(metodo, parametros, identificador) # Si existe el método, lo invoca
            else:
                return self.error_response(-32601, "Method not found", identificador) # Respuesta de error
        else:
            metodo = self.methods.get(nombre_metodo)
            if metodo:
                return self.invoke_method(metodo, parametros, None)
            else:
                return self.error_response(-32601, "Method not found", None)

    '''Función de invocación del método'''
    def invoke_method(self, metodo, parametros, identificador):

        try:
            if (metodo.__name__  in self.variable_methods): # Verifica si hay argumentos variables
                if (len(parametros) > 1): # Asegura que haya más de un parámetro
                    resultado = metodo(*parametros) # Llamada al método con los parámetros
                    return self.success_response(resultado, identificador) # Llamada a la respuesta exitosa
                else:
                    return self.error_response(-32602, "Invalid params", identificador)    
            if len(parametros) != metodo.__code__.co_argcount: # Sigue si no hay parámetros variables y verifica la cantidad recibida
                return self.error_response(-32602, "Invalid params", identificador)
            if isinstance(parametros, dict): # Si los parámetros son un diccionario
                resultado = metodo(**parametros) # Llama al método con parámetros nombrados
            else:
                resultado = metodo(*parametros) # De lo contrario, llama al método con parámetros posicionales

            return self.success_response(resultado, identificador)
        except Exception as e: # Si ocurre alguna excepción durante la invocación
            return self.error_response(-32603, "Internal error", identificador, str(e))
        
    '''Función para crear una respuesta de éxito en formato JSON-RPC'''
    def success_response(self, result, id):
        return {
            "jsonrpc": "2.0",
            "result": result,
            "id": id
        }

    '''Función para crear una respuesta de error en formato JSON-RPC'''
    def error_response(self, code, message, id, data=None):
        error = {
            "code": code,
            "message": message
        }
        if data:
            error["data"] = data
        return {
            "jsonrpc": "2.0",
            "error": error,
            "id": id
        }

    '''Función para enviar una respuesta de error al cliente'''
    def send_error_response(self, skt, code, message, id, data=None):
        response = self.error_response(code, message, id, data)
        skt.sendall(json.dumps(response).encode('utf-8'))

