import requests

# URL del servidor NodeMCU (ajusta la IP según tu configuración)
url = "http://192.168.16.236/get_sensor"

# Realizar la solicitud GET al servidor NodeMCU
response = requests.get(url)

# Verificar si la solicitud fue exitosa
if response.status_code == 200:
    # Si la respuesta es exitosa, parsear el JSON
    data = response.json()
    temperatura = data.get("temperatura")
    humedad = data.get("humedad")
    
    # Mostrar los resultados
    print(f"Temperatura: {temperatura} °C")
    print(f"Humedad: {humedad} %")
else:
    # Si la solicitud falló, mostrar un mensaje de error
    print(f"Error al obtener los datos. Código de estado: {response.status_code}")