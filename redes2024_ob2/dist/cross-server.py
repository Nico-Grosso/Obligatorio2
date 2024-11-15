from jsonrpc_redes import connect

def test_client():
    print('=============================')
    print('Iniciando pruebas de casos sin errores (S1).')

    connS1 = connect('200.0.0.10', 8080)
    connS3 = connect('200.0.0.10', 8080)
    
    result = connS1.add(5,3)
    assert result == 8
    print('Test suma completado (2).')

    result = connS3.add(5,4)
    assert result == 9
    print('Test suma completado (2). Cliente S3')

    result = connS1.add(4,3,2,1)
    assert result == 10
    print('Test suma completado (4).')

    result = connS1.add(5,4, notify=True)
    assert result == None
    print('Test notificación completado.')

    result = connS1.add(notify=True)
    assert result == None
    print('Test notificación sin params completado.')

    result = connS1.generate_email_user('francisco', 'gmail')
    assert result == 'francisco@gmail.com'
    print('Test email completado.')

    result = connS1.generate_password('cont',2)
    assert len(result) == 6
    print('Test contraseña completado.')

    result = connS1.multiplos_de_n([1,2,3,4,5,6,7,8,9,10],3)
    assert result == [3,6,9]
    print('Test método que retorna una lista completado.')

    result = connS1.generate_email_user(user_name='francisco', server='gmail')
    assert result == 'francisco@gmail.com'
    print('Test de múltiples parámetros con nombres completado')

    print('=============================')
    print('Pruebas de casos sin errores completadas.')
    print('=============================')
    print('Iniciando pruebas de casos con errores.')

    try:
        result = connS1.add()
    except Exception as e:
        print('Llamada incorrecta sin parámetros. Genera excepción necesaria.')
        print(e)
    else:
        print('ERROR: No lanzó excepción.')

    try:
        result = connS1.non_method(5, 6)
    except Exception as e:
        print('Llamada a método inexistente. Genera excepción necesaria.')
        print(e)
    else:
        print('ERROR: No lanzó excepción.')

    try:
        result = connS1.generate_password(1, 2)
    except Exception as e:
        print('Llamada incorrecta genera excepción interna del servidor.')
        print(e)
    else:
        print('ERROR: No lanzó excepción.')

    try:
        result = connS1.generate_password('cont', largo=2)
    except Exception as e:
        print('Llamada incorrecta genera excepción en el cliente.')
        print(e)
    else:
        print('ERROR: No lanzó excepción.')

    print('=============================')
    print('Iniciando pruebas de casos sin errores (S2).')

    connS2 = connect('200.100.0.15', 8080)
    connS4 = connect('200.100.0.15', 8080)

    result = connS2.is_sum_even(3,4)
    assert result == False
    print('Test resultado de suma par completado')

    result = connS4.is_sum_even(3,4)
    assert result == False
    print('Test resultado de suma par completado Cliente S4')

    result = connS2.is_sum_even(5,4, notify=True)
    assert result == None
    print('Test notificación completado.')

    result = connS2.get_ocurrences('abad','a')
    assert result == 2
    print('Test ocurrencias completado')

    result = connS2.multiplicar(5,3)
    assert result == 15
    print('Test multiplicación completado (2)')

    result = connS2.multiplicar(5,3,2)
    assert result == 30
    print('Test multiplicación completado (3)')

    print('=============================')
    print('Pruebas de casos sin errores completadas.')
    print('=============================')
    print('Iniciando pruebas de casos con errores.')


    try:
        result = connS2.is_sum_even()
    except Exception as e:
        print('Llamada incorrecta sin parámetros. Genera excepción necesaria.')
        print(e)
    else:
        print('ERROR: No lanzó excepción.')


    try:
        result = connS2.non_method(5, 6)
    except Exception as e:
        print('Llamada a método inexistente. Genera excepción necesaria.')
        print(e)
    else:
        print('ERROR: No lanzó excepción.')

    try:
        result = connS2.is_sum_even('e', 2)
    except Exception as e:
        print('Llamada incorrecta genera excepción interna del servidor.')
        print(e)
    else:
        print('ERROR: No lanzó excepción.')

    try:
        result = connS2.get_ocurrences('abad',caracter='a')
    except Exception as e:
        print('Llamada incorrecta genera excepción en el cliente.')
        print(e)
    else:
        print('ERROR: No lanzó excepción.')


    print('=============================')
    print("Pruebas de casos con errores completadas.")

if __name__ == "__main__":
    test_client()