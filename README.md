🚀 Funcționalități Principale

Modul AUTO: Reglarea unghiului servomotorului proporțional cu temperatura citită de senzorul digital DS18B20 (interpolare liniară între 20°C și 30°C).

Modul MANUAL: Controlul direct al servomotorului pe baza valorii analogice citite de la un potențiometru.

Interfață HMI (Serială & LCD): Monitorizare locală pe un afișaj alfanumeric și un meniu interactiv accesibil printr-un terminal serial (ex: CoolTerm) pentru schimbarea modurilor de lucru.

Managementul Stării (Event-Driven): Funcție de ON/OFF a sistemului comandată dintr-un buton fizic, implementată eficient pentru a elimina buclele de polling.

🧠 Concepte FreeRTOS Implementate

Sistemul este împărțit în 5 task-uri paralele (Temperatură, Tensiune, Servo, Serial, System State), coordonate prin mecanisme avansate specifice sistemelor de operare în timp real:

Cozi de Mesaje (Queues): Transmiterea asincronă, atomizată și sigură a variabilelor float de la senzorii de citire către actuator.

Mutex-uri: Excluderea mutuală pentru protejarea resursei partajate (ecranul LCD), prevenind coliziunile de scriere între task-urile concurente.

Secțiuni Critice: Protejarea temporizărilor stricte ale protocolului 1-Wire în timpul citirii senzorului de temperatură, prevenind întreruperile din partea planificatorului (scheduler).

| Componentă | Conexiune / Pin | Detalii Tehnice |
| :--- | :--- | :--- |
| **Microcontroler** | dsPIC33 | Ceas intern (FRC), Arhitectură 16-biți |
| **Senzor Temperatură** | RB2 | Model DS18B20 (Protocol 1-Wire) |
| **Potențiometru** | RB3 (AN5) | Citire ADC (Conversie pe 12 biți) |
| **Servomotor** | RB10 (OC1) | Semnal PWM 50Hz generat hardware prin Timer 2 |
| **Buton Control S2** | RB7 (INT0) | Declansare întrerupere hardware la apăsare |
| **Ecran LCD** | Port Paralel | Afișare parametri și status operațional |
| **LED Stare** | RB11 | Indicator vizual operat în logică negativă |
| **UART (RS232)** | RX / TX | Comunicație cu PC-ul la 9600 baud rate |
