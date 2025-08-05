# park_IT

parl IT was a project developed in the second semester of 2023/2024, for the Mobile and Pervasive Computing Course in FCT NOVA.
The group members are Bruno David (@BrunoDavidES), Gonçalo Cerveira (@GoncaloCerveira) and Gonçalo Silva (@GoncaloABdaSilva).
The final grade was 18.6 out of 20.0.

We implemented a parking lot management system for companies, designed to enhance the parking experience and ensure safety in the lot for all users. The developed system works as proof of concept.

The system integrates ESP32 microcontrollers (C++) for managing environmental sensors (gas, temperature, humidity), controlling actuators (LEDs, buzzers), and handling RFID-based access control. A local web server (Python/Flask) routes HTTP communication between ESP32's and Firebase, and ensures system resilience with offline data caching. Firebase provides real-time cloud database, user authentication, and trigger-based synchronization. A Flutter mobile app enables user login, parking space reservation, and real-time monitoring of sensor data and system status through streams and UI updates.

Front-End repository: https://github.com/BrunoDavidES/Frontend_park_IT
Demonstrative videos: https://drive.google.com/drive/folders/1_QIB7fODKVsESPs6ju0_MadK-rw5igUR

The report file (Final SCMU Report - park IT.pdf) explains the project in more detail.
