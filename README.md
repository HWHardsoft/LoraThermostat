# Lora driven room thermostat controlled via Google Calendar

The task for this project was to remotely control the temperature in 2 club rooms. Both rooms are used for events and should only be heated for these events in order to save energy. Previously, 2 very simple thermostats were installed in these rooms and someone always had to manually raise the temperature on the thermostats before the relevant events and turn it down again after the event. We wanted a solution where you can enter the events in a Google Calendar in advance and then the corresponding thermostats for the respective room are automatically raised or lowered again.

![old thermostat](https://hackster.imgix.net/uploads/attachments/1700248/old_thermostat_Nz5WsYpbOr.jpg?auto=compress%2Cformat&w=740&h=555&fit=max)

Unfortunately, there is no WLAN available in these rooms. Lora radio was therefore used. The data from the Google Calendar is fetched via a gateway located in an office 200 meters away and converted into Lora radio protocols.

![Blockdiagram](https://cdn.hackaday.io/images/5423011711733784854.jpg)

## Operation modes:

Manual operation:In this operating mode, the room temperature can be freely selected within certain limits using the thermostat's touchscreen. The "Automatic" entry in the Google Calendar can be used to switch back to automatic mode.

Automatic mode:In this operating mode, the Google Calendar automatically switches between a presettable setback temperature and a presettable heating temperature. These two above-mentioned temperatures can be set separately in the thermostat's options menu.

## Thermostates:

[AZ-Touch Feather](https://www.hwhardsoft.de/english/projects/az-touch-feather/) kits were used for the thermostats. Each of the thermostats is equipped with a 2.8 inch ili9341 touchscreen.

![new thermostat](https://cdn.hackaday.io/images/2816151711734590566.jpg)

As the AZ-Touch kit is not designed for 230V supply voltage, a small additional board was designed which, in addition to a 230V ACDC converter, also contains a relay for controlling the heating and a DS18B20 1wire sensor for measuring the room temperature.

![additional pcb](https://cdn.hackaday.io/images/3035021711734909985.jpg)

The circuit board of the AZ-Touch has been slightly modified so that the ACDC module fits into the housing:

![test run without enclosure](https://cdn.hackaday.io/images/5410221711737920500.jpg)

## Gateway:
The gateway was mounted on a perforated grid plate. Essentially, the gateway consists of an ESP32 DEV KIT C module as CPU and an Adafruit RFM96W LoRa Radio Transceiver 433 MHz Breakout. The connection to the Internet is established via the WiFi of the ESP32 module.

![Gateway](https://cdn.hackaday.io/images/180671711738295464.jpg)

If you want to save yourself this work, you can alternatively use an Adafruit Huzzah32 and an Adafruit LoRa Radio FeatherWing - RFM95W 433 MHz.

## Google Calendar
The heating can be controlled remotely via a Google Calendar. I used the script and instructions from [Seweryn Tałaj](https://github.com/seweryntalaj/Google-Calendar-with-Node-MCU) for this.

![calendar](https://hackster.imgix.net/uploads/attachments/1700816/grafik_g12jCfeUc6.png?auto=compress%2Cformat&w=740&h=555&fit=max)

If the heating is now to be activated, a date is simply entered in the calendar. The appointment must contain the term "Room1" or "Room2" in the subject line as the key word in the subject line. You can of course adapt the program in the gateway accordingly and assign better key words. Please set the time so that there is enough time to heat up - in other words at least 2-3 hours longer. Otherwise leave the description of the date blank and do not invite any participants.

