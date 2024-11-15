import json

def recvall(skt):
    # Recibe datos de un socket hasta que se recibe un mensaje JSON válido
    buffer = bytearray()  # Crea un buffer para almacenar los datos recibidos
    esValido = False  # Inicializa una variable para controlar la validez del mensaje

    while not esValido:
        # Recibe hasta 4096 bytes del socket
        part = skt.recv(4096)
        if not part:
            return None  # Retorna None si no se recibe más datos (el cliente se desconectó)
        
        buffer.extend(part)  # Añade los datos recibidos al buffer

        try:
            # Intenta parsear el contenido del buffer como JSON
            json.loads(buffer.decode('utf-8'))
            esValido = True  # Si se puede parsear, establece esValido a True
        except json.JSONDecodeError:
            # Si ocurre un error de parseo, continúa recibiendo datos
            pass
            
    return buffer.decode('utf-8')  # Retorna el contenido del buffer como una cadena
