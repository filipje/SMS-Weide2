# SMS-Weide2
GPIO15listens for pulses
Pulse 1 starts a timer (20 Sec)
exta incoming pulses are added,
the number of pulses defines the alarmtext, 
alarm text is sent to //sms's.

temperature sensor DS18B20:
keeps track of the lowest T,
when +2 °C is detected, a sms is sent with Lowest T.
