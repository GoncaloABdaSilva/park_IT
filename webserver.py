import time
import firebase_admin
from firebase_admin import db, credentials
from flask import Flask, request, jsonify
import json
import requests
import threading

cred = credentials.Certificate("credentials.json")
firebase_admin.initialize_app(cred, {"databaseURL":"https://parkit-scmu-2324-default-rtdb.europe-west1.firebasedatabase.app/"})

spot_1_2_id = "172.20.10.14:5000"
main_esp_id = "172.20.10.4:5000"

app = Flask(__name__)

@app.route('/set', methods=['POST'])
def setRequest():
    if request.method == 'POST':
        data = request.json
        value = data['value']
        print(data)
        db.reference("/" + data['name']).set(value) 
        return jsonify({'message': 'Received JSON data successfully'})
    else:
        return jsonify({'error': 'Only POST requests are allowed'})
    
@app.route('/get', methods=['POST'])
def getRequest():
    if request.method == 'POST':
        data = request.json
        print(data)
        print (db.reference("/" + data['name']).get() )
        return jsonify({'response': db.reference("/" + data['name']).get() })
    else:
        print("erro, mas entra")
        return jsonify({'error': 'Only POST requests are allowed'})
    
@app.route('/entry', methods=['POST'])
def setEntry():
    if request.method == 'POST':
        data = request.json
        rfid = data['name']
        date = data['time']
        print(data)

        userId = ""
        objects = db.reference("/usersList").get()

        for key, obj in objects.items():
            if isinstance(obj, dict) and 'rfid' in obj and obj['rfid'] == rfid:
                userId = key
                break

        accesses = db.reference("/counters/accessHistory/" + userId).get()
        if accesses == None :
            accesses = 0
            db.reference("/counters/accessHistory/" + userId).set(accesses)

        db.reference("/accessHistory/" + userId + "/" + str(accesses) + "/entry").set(date)

        db.reference("/usersList/" + userId + "/last_entry").set(date)
        db.reference("/usersInPark/" + userId).set(date)

        counter = db.reference("/counters/usersInPark").get()
        db.reference("/counters/usersInPark").set(counter + 1)
        return jsonify({'message': 'Received JSON data successfully'})
    else:
        return jsonify({'error': 'Only POST requests are allowed'})

@app.route('/exit', methods=['POST'])
def setExit():
    if request.method == 'POST':
        data = request.json
        rfid = data['name']
        exitDate = data['time']
        print(data)

        userId = ""
        objects = db.reference("/usersList").get()

        for key, obj in objects.items():
            if isinstance(obj, dict) and 'rfid' in obj and obj['rfid'] == rfid:
                userId = key
                break
        
        accesses = db.reference("/counters/accessHistory/" + userId).get()
        db.reference("/counters/accessHistory/" + userId).set(accesses + 1)

        counter = db.reference("/counters/usersInPark").get()
        db.reference("/counters/usersInPark").set(counter - 1)

        db.reference("/usersInPark/" + userId).delete()

        db.reference("/accessHistory/" + userId + "/" + str(accesses) + "/exit").set(exitDate)

        return jsonify({'message': 'Received JSON data successfully'})
    else:
        return jsonify({'error': 'Only POST requests are allowed'})


def sendPostRequest():
    time.sleep(1)
    with app.app_context():
    
        json_spot0 = db.reference("/parkingSpots/0").get()
        json_spot0['spot'] = 0

        json_spot1 = db.reference("/parkingSpots/1").get()
        json_spot1['spot'] = 1

        json_spot0 = json.dumps(json_spot0)
        json_spot1 = json.dumps(json_spot1)

        esp32_url = f"http://{spot_1_2_id}/update"

        response = requests.post(esp32_url, data=json_spot0, headers={'Content-Type': 'application/json'}) 
        print("Response:", response.text)   

        response = requests.post(esp32_url, data=json_spot1, headers={'Content-Type': 'application/json'}) 
        print("Response:", response.text)


        esp32_url = f"http://{main_esp_id}/update"

        data = {}
        data['name'] = "reserveRFIDs"
        data['value'] = db.reference("/reserveUsersRFID").get()
        dataJson = json.dumps(data)

        response = requests.post(esp32_url, data = dataJson, headers={'Content-Type': 'application/json'})
        print("Response:", response.text)


        data = {}
        data['name'] = "usersRFIDs"
        data['value'] = db.reference("/usersRFID").get()
        dataJson = json.dumps(data)

        response = requests.post(esp32_url, data = dataJson, headers={'Content-Type': 'application/json'})
        print("Response:", response.text)


        data = {}
        data['name'] = "constants"
        data['access'] = db.reference("/SensorState/access").get()
        data['gates'] = db.reference("/SensorState/open_gates").get()
        data['gas'] = db.reference("/SensorState/air_quality").get()
        data['temp_hum'] = db.reference("/SensorState/humidity_and_temperature").get()
        dataJson = json.dumps(data)

        response = requests.post(esp32_url, data = dataJson, headers={'Content-Type': 'application/json'})
        print("Response:", response.text)


        data = {}
        data['name'] = "gasThreshold"
        data['value'] = db.reference("/thresholdValues/gas_danger").get()
        dataJson = json.dumps(data)

        response = requests.post(esp32_url, data = dataJson, headers={'Content-Type': 'application/json'})
        print("Response:", response.text)


        data = {}
        data['name'] = "tempThreshold"
        data['value'] = db.reference("/thresholdValues/temperature_danger").get()
        dataJson = json.dumps(data)

        response = requests.post(esp32_url, data = dataJson, headers={'Content-Type': 'application/json'})
        print("Response:", response.text)


        data = {}
        data['name'] = "humiThreshold"
        data['value'] = db.reference("/thresholdValues/humidity_danger").get()
        dataJson = json.dumps(data)

        response = requests.post(esp32_url, data = dataJson, headers={'Content-Type': 'application/json'})
        print("Response:", response.text)


def spotsListener(event):
    print(event.path)
    path = event.path.split('/')
    print(path)
    if (path[1] != ''):
        print(path)
        json_spot = db.reference('/parkingSpots/' + path[1]).get()
        json_spot['spot'] = path[1]
        dataJson = json.dumps(json_spot)

        esp32_url = f"http://{spot_1_2_id}/update"

        #Teríamos uma lista de ips de spots, para obter indice da pos do ip, basta dividir spot por 2 e ficar só com o Integer
        response = requests.post(esp32_url, data = dataJson, headers={'Content-Type': 'application/json'})
        
        print("Response:", response.text)

parkingSpots = db.reference("/parkingSpots/")
parkingSpots.listen(spotsListener)

def reserveRFIDsListener(event):
    print(event.path)
    path = event.path.split('/')
    print(path)
    if (path[1] != ''):
        data = {}
        data['name'] = "reserveRFIDs"
        data['value'] = db.reference("/reserveUsersRFID").get()
        dataJson = json.dumps(data)

        esp_url = f"http://{main_esp_id}/update"

        response = requests.post(esp_url, data = dataJson, headers={'Content-Type': 'application/json'})
        print("Response:", response.text)

reserveRFIDs = db.reference('/reserveUsersRFID')
reserveRFIDs.listen(reserveRFIDsListener)

def usersRFIDsListener(event):
    print(event.path)
    path = event.path.split('/')
    print(path)
    if (path[1] != ''):
        data = {}
        data['name'] = "usersRFIDs"
        data['value'] = db.reference("/usersRFID").get()
        dataJson = json.dumps(data)

        esp_url = f"http://{main_esp_id}/update"

        response = requests.post(esp_url, data = dataJson, headers={'Content-Type': 'application/json'})
        print("Response:", response.text)

usersRFIDs = db.reference("/usersRFID")
usersRFIDs.listen(usersRFIDsListener)


def thresholdsListener(event):
    print(event.path)

    print(event.data)
    received = list(event.data)
    name = received[0]
    
    if (name != ''):
        data = {}

        if name == "gas_danger":
            data["name"] = "gasThreshold"
        elif name == "humidity_danger":
            data["name"] = "humiThreshold"
        elif name == "temperature_danger":
            data["name"] = "tempThreshold"
 
        data['value'] = event.data[name]

        dataJson = json.dumps(data)

        esp_url = f"http://{main_esp_id}/update"

        response = requests.post(esp_url, data = dataJson, headers={'Content-Type': 'application/json'})
        print("Response:", response.text)
        

thresholds = db.reference("/thresholdValues")
thresholds.listen(thresholdsListener)


if __name__ == '__main__':    
    request_thread = threading.Thread(target=sendPostRequest)
    request_thread.start()
    
    app.run(debug=True, host='172.20.10.2', port=5000)
