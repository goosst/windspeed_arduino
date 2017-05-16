/*
  author: SG
  creation date: 8 May 2017
  hardware: arduino due

  read out wind speed sensor
  Eltako WS Windsensor

*/

// histogram library downloaded from arduino playground
//#include <histogram.h>
#include <DueFlashStorage.h>

const byte pin_led = 13;
const byte pin_interrupt = 32;

volatile long timestamps[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile byte timestamps_index = 0;
volatile bool timestamp_newdata = false;

// copy to avoid updates during processing
volatile long timestamps_cp[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile byte timestamps_index_cp = 0;

int index_buffer = 0;
long time_delta[3] = {0, 0, 0};
long temporary;
float sensor_rps;
float v_wind_kph;

int time_delay = 1000;
bool first_run = false;


DueFlashStorage dueFlashStorage;

struct histogram_flash {
  float hist_counter[19];
  uint8_t hist_buckets[19] = {0, 2, 4, 6, 8, 10, 12, 14, 15, 16, 18, 20, 25, 30, 35, 40, 45, 50, 60};
  float hist_total;
};

histogram_flash Flash_histogram;


void setup() {
  //start serial connection
  Serial.begin(9600);
  pinMode(pin_interrupt, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin_interrupt), get_timestamp, RISING);

  pinMode(pin_led, OUTPUT);
  first_run = false;

  // write configuration struct to flash at adress 4
  byte b2[sizeof(Flash_histogram)]; // create byte array to store the struct
  memcpy(b2, &Flash_histogram, sizeof(Flash_histogram)); // copy the struct to the byte array
  dueFlashStorage.write(4, b2, sizeof(Flash_histogram)); // write byte array to flash


}

void loop() {

  // copy timestamp buffers to avoid updates during processing
  timestamps_index = timestamps_index_cp;
  for ( byte i = 0 ; i < 8 ; i++ ) {
    timestamps[ i ] = timestamps_cp[ i ];
  }

  if (timestamp_newdata) {
    // process data from circular buffer of eight timestamps long
    // two pulses per revolution are given by the windsensor
    // take median value from three delta_times
    timestamp_newdata=false; // set false till new timestamps arrived
    for (byte i = 0; i < 3; i++) {
      index_buffer = timestamps_index - 2 * i;
      if (index_buffer < 0) {
        index_buffer = index_buffer + 8;
      }
      time_delta[i] = timestamps[index_buffer];
      //    Serial.println(timestamps[index_buffer]);

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

      Serial.println("wind speed:");
      Serial.println(v_wind_kph);

      // add windpseed to histogram
      for  (byte i = 0; i < 19; i++) {
        if (v_wind_kph >= Flash_histogram.hist_buckets[i]) {
          Flash_histogram.hist_counter[i] = Flash_histogram.hist_counter[i] + 1;
        }
      }
      Flash_histogram.hist_total = Flash_histogram.hist_total + 1;

    }
  }

  for (byte i = 0; i < 19; i++) {
    Serial.println(Flash_histogram.hist_counter[i]);
  }

  Serial.println("----");

  delay(time_delay);
}

void get_timestamp() {
  timestamp_newdata = true;
  timestamps_index_cp++;
  if (timestamps_index_cp > 7) {
    timestamps_index_cp = 0;
  }

  timestamps_cp[timestamps_index_cp] = micros();
}


