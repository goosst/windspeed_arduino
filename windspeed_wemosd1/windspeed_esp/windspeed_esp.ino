/*
  author: SG
  creation date: 16 sept 2017
  hardware: wemos D1

  description: read out wind speed sensor, Eltako WS Windsensor
  two pulses per revolution are obtained from the sensor, timestamps of pulses are collected, processed and speed is added in a histogram
  every x amount of time the histogram gets written to the flash. No eeprom available on due, flash is reset when reprogramming arduino (be aware!)

*/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
//#include "ThingSpeak.h"

const byte pin_reset = 5;
const byte pin_interrupt = 4;
const int ledPin = 2;

volatile long timestamps[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile byte timestamps_index = 0;
volatile bool timestamp_newdata = false;

// copies to avoid updates during processing
volatile long timestamps_cp[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile byte timestamps_index_cp = 0;
int index_buffer = 0;
long time_delta[3] = {0, 0, 0};


int time_delay = 1000; // "sample time"
bool first_run;
int counter = 0;

// NVM stuff
// initialize data block
struct histogram_flash {
  float hist_counter[19];
  uint8_t hist_buckets[19];
  float hist_total = 0;
};

histogram_flash Flash_histogram;

// histogram windspeeds
uint8_t buckets[19] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 25, 30, 35, 40, 45, 50, 60, 200};
int address = 0;


const char* ssid = "test";
const char* password = "test";


//unsigned long myChannelNumber = 418746;
//const char * myWriteAPIKey = "PYSYY3XNYC630D8O";
WiFiClient  client;
//IPAddress ip(192, 168, 0, 177);
IPAddress ip(10, 24,1,3);
IPAddress gateway(192, 168, 0, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 0, 254);

//const int sleepTimeS = 10;
WiFiServer server(81);


int sensorPin = A0;//temp sensor
float resistance;

void setup()
{

  pinMode(14, OUTPUT);

  Serial.begin(9600);
  //  delay(100);


  //set DIO
  pinMode(pin_interrupt, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin_interrupt), get_timestamp, RISING);
  pinMode(pin_reset, INPUT_PULLUP); // pin to reset histogram

  //  counter = 0; // counter to enable writing to flash

  // read data + do sanity check on data in NVM
  EEPROM.begin(sizeof(Flash_histogram));
  // check if flash data should be used for initialization of data
  histogram_flash Flash_histogram_temp;
  //EEPROM.readBlock(address, Flash_histogram_temp);
  EEPROM.get(address, Flash_histogram_temp);
  EEPROM.end();

  bool use_flash_data = true;
  for (byte i = 0; i < 19; i++) {
    if (abs(Flash_histogram_temp.hist_buckets[i] - buckets[i]) > 0.01) {
      use_flash_data = false;
    }
  }

  if (use_flash_data) {
    //    memcpy(&Flash_histogram, b, sizeof(Flash_histogram_temp));
    Flash_histogram = Flash_histogram_temp;
  }
  else {
    Flash_histogram.hist_total = 0;

    for (byte i = 0; i < 19; i++) {
      Flash_histogram.hist_buckets[i] = buckets[i];
      Flash_histogram.hist_counter[i] = 0;
    }
    EEPROM.begin(sizeof(Flash_histogram));
    EEPROM.put(address, Flash_histogram);
    EEPROM.end();
  }


  // print data
  //  for (byte i = 0; i < 19; i++) {
  //    Serial.println(Flash_histogram.hist_counter[i]);
  //  }
  //  Serial.println(Flash_histogram.hist_total);
  //  Serial.println("---------");

  //    Serial.println("ESP8266 in sleep mode");
  //  ESP.deepSleep(sleepTimeS * 1000000);

  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);


}


void loop() {

  delay(100);

  digitalWrite(14, LOW);

  Serial.println("temp sensor");
  // read the value from the temp sensor:
  int sensorValue = analogRead(sensorPin);
  float sensorvoltage = 3.3 / 3.2 * float(sensorValue) / 1024.0f;


  Serial.println(sensorvoltage);

  if (sensorvoltage < 1) {
    resistance = 10 * sensorvoltage / (1 - sensorvoltage);
    Serial.println(resistance);
  }
  Serial.println("----");


  // calculate wind speed, loop for a few seconds
  for (byte i = 0; i < 4; i++) {

    //     Serial.println(Flash_histogram.hist_total);

    timestamp_newdata = false;
    attachInterrupt(digitalPinToInterrupt(pin_interrupt), get_timestamp, RISING);

    delay(500); // allow some time to get pulses in

    long temporary;
    float sensor_rps;
    float v_wind_kph;

    //reset histogram?
    bool hist_reset = !digitalRead(pin_reset);


    if (hist_reset) {
      Flash_histogram.hist_total = 0;

      for (byte i = 0; i < 19; i++) {
        Flash_histogram.hist_buckets[i] = buckets[i];
        Flash_histogram.hist_counter[i] = 0;
      }
    }


    Flash_histogram.hist_total = Flash_histogram.hist_total + 1;

    Serial.print(Flash_histogram.hist_total );
    //  Serial.println();

    // copy timestamp buffers to avoid updates during data-processing
    timestamps_index = timestamps_index_cp;
    for ( byte i = 0 ; i < 8 ; i++ ) {
      timestamps[ i ] = timestamps_cp[ i ];
      //      Serial.println(timestamps[i]);
    }

    Serial.println(timestamp_newdata);
    if (timestamp_newdata) {
      // process data from circular buffer of eight timestamps long
      // two pulses per revolution are given by the windsensor
      // take median value from three delta_times
      timestamp_newdata = false; // set false till new timestamps arrived
      for (byte i = 0; i < 3; i++) {
        index_buffer = timestamps_index - 2 * i;
        if (index_buffer < 0) {
          index_buffer = index_buffer + 8;
        }
        time_delta[i] = timestamps[index_buffer];
        //          Serial.println(timestamps[index_buffer]);

        index_buffer = timestamps_index - 2 * (i + 1);
        if (index_buffer < 0) {
          index_buffer = index_buffer + 8;
        }
        time_delta[i] = time_delta[i] - timestamps[index_buffer];
        //    Serial.println(timestamps[index_buffer]);
      }

      // sort time_delta buffer
      for (byte j = 0; j < 3; j++)
      {
        for (byte i = 1; i < 3 - j; i++)
        {
          if (time_delta[i - 1] > time_delta[i])
          {
            temporary = time_delta[i];
            time_delta[i] = time_delta[i - 1];
            time_delta[i - 1] = temporary;
          }
        }
      }

      if (time_delta[1] != 0) {
        // take second element out of buffer ( the median)
        sensor_rps = 1000000 / (float)time_delta[1];
        v_wind_kph = 1.761 / (1 + sensor_rps) + 3.013 * sensor_rps;

      }
    }
    else {
      v_wind_kph = 0;
    }

    Serial.print("wind speed: ");
    Serial.println(v_wind_kph);

    // add windpseed to histogram
    for  (int i = 18; i >= 0; i--) {
      if (v_wind_kph >= Flash_histogram.hist_buckets[i]) {
        Flash_histogram.hist_counter[i] = Flash_histogram.hist_counter[i] + 1;
        break;
      }
    }
    Flash_histogram.hist_total = Flash_histogram.hist_total + 1;
  }



  // write every x minutes to NVM
  //  counter++;
  //  if (counter == 500) {
  // EEPROM.writeBlock(address, Flash_histogram);

  EEPROM.begin(sizeof(Flash_histogram));
  EEPROM.put(address, Flash_histogram);
  EEPROM.end();
  Serial.println("write to flash done");

  //    counter = 0;
  //  digitalWrite(ledPin, HIGH);
  //  delay(10);

  //    ThingSpeak.setField(1, v_wind_kph);
  //    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  //  }
  //  digitalWrite(ledPin, LOW);

  // wake up wifi
  WiFi.forceSleepWake();
  delay(1);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet);

  int connection_time = 0;
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED & connection_time < 15)
  {
    delay(500);
    Serial.print(".");
    connection_time = connection_time + 1;
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  for (byte i = 0; i < 10; i++) {
    // Start the server
    server.begin();
    Serial.println("Server started");
    delay(100);

    client = server.available();

    connection_time = 0;
    while (client.connected() & connection_time < 10) {
      connection_time = connection_time + 1;
      delay(100);
      while (client.available() & connection_time < 10) {
        Serial.println("new client");
        String line = client.readStringUntil('\r');
        Serial.print(line);
        client.flush();

        client.println("histogram: speed - samples");
        for (byte i = 0; i < 19; i++) {
          client.print( Flash_histogram.hist_buckets[i]);
          client.print( ",");
          client.println( Flash_histogram.hist_counter[i]);
          //      Flash_histogram.hist_counter[i] = 0;
        }
        client.println("");
        client.println("total histogram counter");
        client.println( Flash_histogram.hist_total);

        client.println("temperature - resistance");
        client.print(resistance);
        client.print(" Ohm");

        Serial.println(resistance);
        Serial.println("ohm");

        connection_time = 999;

      }


    }
    delay(5000);
    server.close();
  }

  //  // Check if a client has connected
  //  for (byte i = 0; i < 10; i++) {
  //
  //    Serial.println("client connected?");
  //    Serial.println(client.available());
  //    Serial.println(client);
  //
  //    if (client.available()) {
  //
  //      // Wait until the client sends some data
  //      Serial.println("new client");
  //
  //      // Read the first line of the request
  //      String req = client.readStringUntil('\r');
  //      Serial.println("request made?");
  //      Serial.println(req);
  //      client.flush();
  //
  //      // Match the request
  //      //    if (req.indexOf("/read") != -1) {
  //      client.println("histogram: speed - samples");
  //      for (byte i = 0; i < 19; i++) {
  //        client.print( Flash_histogram.hist_buckets[i]);
  //        client.print( ",");
  //        client.println( Flash_histogram.hist_counter[i]);
  //        delay(1);
  //        //      Flash_histogram.hist_counter[i] = 0;
  //      }
  //      client.println("");
  //      client.println("total histogram counter");
  //      client.println( Flash_histogram.hist_total);
  //      return;
  //
  //
  //
  //    }
  //
  //    if (!client.connected()) {
  //      Serial.println("disconnecting");
  //      client.stop();
  //    }
  //
  //    delay(2000);
  //    client.stop();
  //
  //
  //  }


  Serial.println("ESP8266 in sleep mode");
  ESP.deepSleep(20 * 1000000);
}


void get_timestamp() {
  timestamp_newdata = true;
  timestamps_index_cp++;
  if (timestamps_index_cp > 7) {
    timestamps_index_cp = 0;
  }

  timestamps_cp[timestamps_index_cp] = micros();
  //  Serial.println("new timestamp");
  //  Serial.println(timestamps_cp[timestamps_index_cp]);
}
