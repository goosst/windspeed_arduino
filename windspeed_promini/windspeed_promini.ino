/*
  author: SG
  creation date: 16 sept 2017
  hardware: arduino pro mini

  description: read out wind speed sensor, Eltako WS Windsensor
  two pulses per revolution are obtained from the sensor, timestamps of pulses are collected, processed and speed is added in a histogram
  every x amount of time the histogram gets written to the flash. No eeprom available on due, flash is reset when reprogramming arduino (be aware!)

*/

#include <EEPROMex.h>


const byte pin_reset = 5;
const byte pin_interrupt = 3;
const int ledPin = 13;

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

void setup()
{
  Serial.begin(9600);
  delay(2000);

  pinMode(pin_interrupt, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin_interrupt), get_timestamp, RISING);
  pinMode(pin_reset, INPUT_PULLUP); // pin to reset histogram

  counter = 0; // counter to enable writing to flash


  // check if flash data should be used for initialization of data
  histogram_flash Flash_histogram_temp;
  EEPROM.readBlock(address, Flash_histogram_temp);

  Serial.println(Flash_histogram_temp.hist_total);

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
    EEPROM.writeBlock(address, Flash_histogram);

  }

  // print data
  for (byte i = 0; i < 19; i++) {
    Serial.println(Flash_histogram.hist_counter[i]);
  }
  Serial.println(Flash_histogram.hist_total);
  Serial.println("---------");

}

void loop() {
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

  //  Serial.print(Flash_histogram.hist_total );
  //  Serial.println();

  // copy timestamp buffers to avoid updates during data-processing
  timestamps_index = timestamps_index_cp;
  for ( byte i = 0 ; i < 8 ; i++ ) {
    timestamps[ i ] = timestamps_cp[ i ];
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

  // print data
  for (byte i = 0; i < 19; i++) {
    Serial.print(buckets[i]);
    Serial.print(":");
    Serial.println(Flash_histogram.hist_counter[i]);
  }
  Serial.println(Flash_histogram.hist_total);
  Serial.println("----");


  // write every x minutes to NVM
  counter++;
  if (counter == 40) {
    EEPROM.writeBlock(address, Flash_histogram);
    Serial.println("write to flash done");
    counter = 0;
    digitalWrite(ledPin, HIGH);
    delay(10);
  }
  digitalWrite(ledPin, LOW);

  delay(time_delay); // give time_delay time to capture new data

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
