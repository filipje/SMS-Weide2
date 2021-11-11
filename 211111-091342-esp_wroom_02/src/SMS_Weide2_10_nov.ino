

/*
  GEERT WEIDE 2 version 10 November 2021
  Complete project details at https://RandomNerdTutorials.com/esp32-sim800l-send-text-messages-sms/
  'ESP32 Wrover Module' in de board manager.
*/

// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = "";
// Your phone number to send SMS:
#define SMS_FILIP  "+32479659282"
#define SMS_GEERT  "+32479279169"



// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb
#include <Wire.h>
#include <TinyGsmClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       25//ipv23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

// GPIO where the DS18B20 is connected to
const int oneWireBus = 2;

//AlarmContact input Pin
#define Alarm_CONTACT        15// GPIO Alarmcontact

#define SerialMon Serial

//AlarmTimer variables
unsigned long startMillisAlarm = 0;  //Start the Alarm timing
const long AlarmTimer = 18000;  //18 Seconds
int AlarmPushCounter = 0;   // counter for the number of AlarmButton presses
int AlarmButtonOn = 0;         // current state of the AlarmButton
int lastAlarmButtonOn = 0;     // previous state of the AlarmButton
// alarm texts
String Alarms[] = {"NotUsed", "W2 = te warm!", "W2:doe de deur toe/open!", "W2 = te koud!", "W2:Platte batterij!", "W2:nogal donker he?", "W2:komt Vos?"};
int PulseTimer = 0;     // Start timer is running
// Flag variable to keep track if alert SMS was sent or not
bool smsSentAlarm = false;

//Temperature Timer variables
unsigned long startMillisTemp = 0;  //Start the Temperature timing
const long TempTimer = 20000; //  20 S ....4Hr=14.400 Seconds
//int TimerTempDone = 0;     // timer is done
int TempTimerStart = 0;     // Temperature timer is running
bool smsSentLowT = false;
float minT = 80 ;

// Set serial for AT commands (to SIM800 module)
#define SerialAT  Serial1
// Define the serial console for debug prints, if needed
//#define DUMP_AT_COMMANDS
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif
#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00
bool setPowerBoostKeepOn(int en) {
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;

}
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  // Start the DS18B20 Temperature sensor
  sensors.begin();

  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  pinMode(Alarm_CONTACT, INPUT);

  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  SerialMon.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart
  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }
}

void loop() {
  sensors.setResolution(9);
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  if (PulseTimer == LOW) {//Start the temperatureTimer when SMSpulsing is done
    SerialMon.print(PulseTimer);
    SerialMon.print(".");
    SerialMon.print(temperatureC);
    SerialMon.print(".");
    SerialMon.print(minT);
    SerialMon.println("--");
    if (TempTimerStart == HIGH) {//verify temperature changes
      startMillisTemp = millis ();
      TempTimerStart = LOW;
    }
    else {
      if (millis() - startMillisTemp >= TempTimer) { //test whether the TempTimer has elapsed and look for the minimum temperature
        TempTimerStart = HIGH;
        minT = min (temperatureC, minT) ;
        if ((temperatureC - minT > 2) && !smsSentLowT) {//send smsLowT
          String smsMessageT = String("W2 LowestTemp = ") + String(minT) + String(" C");
          if (modem.sendSMS(SMS_FILIP, smsMessageT)) {
            delay (2000);
            //modem.sendSMS(SMS_GEERT, smsMessageT);
            SerialMon.println(smsMessageT);
            String smsMessageT = String("Null");//weg?
            smsSentLowT = HIGH;
            minT = temperatureC ;
          }
          else {
            SerialMon.println("SMSTemp failed to send Temperature");
            SerialMon.println(smsMessageT);
            smsSentLowT = HIGH;
            minT = temperatureC ;
          }
        }
        else {//verify again for the lowest temperature
          smsSentLowT = LOW;
        }
      }
    }
  }

  // check number of alarmButton pulses for 10 sec..
  if (AlarmPushCounter >= 7 + 2) { //number of alarms +2 for initialisation
    AlarmPushCounter = 0;
    PulseTimer = LOW;
    //digitalWrite(ledPin, LOW);
  }
  counting();
  if (AlarmPushCounter == 1) {
    PulseTimer = HIGH;
    startMillisAlarm = millis();
    AlarmPushCounter = 2;
    smsSentAlarm = false;
  }
  if (PulseTimer == HIGH) {// wait 10 sec and send sms if pushes > 2

    if ((millis() - startMillisAlarm >= AlarmTimer) && !smsSentAlarm) { //test whether the AlarmTimer has elapsed
      SerialMon.print("TimerDone");
      SerialMon.print("number of Alarm pulses: ");
      SerialMon.println(AlarmPushCounter);
      if (AlarmPushCounter > 2) {
        String smsMessage = String(Alarms[AlarmPushCounter - 2] + " " + temperatureC + "C");
        if (modem.sendSMS(SMS_FILIP, smsMessage)) {
          delay (1000);
          //modem.sendSMS(SMS_GEERT, smsMessage);
          SerialMon.println(smsMessage);
          smsSentAlarm = true;
          String smsMessage = String ("Sent");
          AlarmPushCounter = 0;
          PulseTimer = LOW;
        }
        else {

          SerialMon.println("SMS failed to send Alarm");
          SerialMon.println(smsMessage);
          smsSentAlarm = true;// weglaten
          AlarmPushCounter = 0;
          PulseTimer = LOW;
        }
      }
      else  {
        PulseTimer = LOW;//only 1 pulse was sent
        AlarmPushCounter = 0;
      }
    }
  }
  delay (50);
}

void counting() {
  // check number of alarmButton pulses for 10 sec..
  AlarmButtonOn = digitalRead(Alarm_CONTACT);// read the pushAlarmButton input pin:
  if (AlarmButtonOn != lastAlarmButtonOn) {// compare the AlarmButtonOn to its previous state
    if (AlarmButtonOn == HIGH) {// if the state has changed, increment the counter
      AlarmPushCounter++;
      SerialMon.println("C=on");
      SerialMon.print("number of Alarm pushes: ");
      SerialMon.println(AlarmPushCounter);
    }
    else {
      SerialMon.println("C=off");// if the current state is LOW then the AlarmButton went from on to off:
    }
    delay(50);// Delay a little bit to avoid bouncing
  }
  lastAlarmButtonOn = AlarmButtonOn;// save the current state as the last state, for next time through the loop
}
