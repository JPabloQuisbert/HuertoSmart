import requests

# URL del servidor NodeMCU (ajusta la IP según tu configuración)
url = "http://192.168.16.236/get_pines"

# Realizar la solicitud GET al servidor NodeMCU
response = requests.get(url)

# Verificar si la solicitud fue exitosa
if response.status_code == 200:
    # Si la respuesta es exitosa, parsear el JSON
    data = response.json()
    #print(data)
    for pines in data["pines"]:
        pin = pines["pin"]
        nombre = pines["nombre"]
        estado = pines["estado"]
        print(f"Pin {pin}: {nombre}, estado:{estado}")
else:
    # Si la solicitud falló, mostrar un mensaje de error
    print(f"Error al obtener los datos. Código de estado: {response.status_code}")
