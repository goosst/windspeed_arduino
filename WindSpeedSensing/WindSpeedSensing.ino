/*
  author: SG
  creation date: 8 May 2017
  hardware: arduino due

  description: read out wind speed sensor, Eltako WS Windsensor
  two pulses per revolution are obtained from the sensor, timestamps of pulses are collected, processed and speed is added in a histogram
  every x amount of time the histogram gets written to the flash. No eeprom available on due, flash is reset when reprogramming arduino (be aware!)

  21 May 2017: added pin_reset to be able to reset the histogram data hardware wise (connecting gnd to pin_reset)
  27 May 2017: bugfix histogram, added sleep modes to reduce power consumption
*/


#include <DueFlashStorage.h>

const byte pin_reset = 42;
const byte pin_interrupt = 32;
const int ledPin = 13;

volatile long timestamps[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile byte timestamps_index = 0;
volatile bool timestamp_newdata = false;

// copy to avoid updates during processing
volatile long timestamps_cp[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile byte timestamps_index_cp = 0;
int index_buffer = 0;
long time_delta[3] = {0, 0, 0};


int time_delay = 10000; // "sample time"
bool first_run;
int counter = 0;

DueFlashStorage dueFlashStorage;

// flash stuff
// initialize flash struct
struct histogram_flash {
  float hist_counter[19];
  uint8_t hist_buckets[19];
  float hist_total = 0;
};
histogram_flash Flash_histogram;

// histogram windspeeds
uint8_t buckets[19] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 25, 30, 35, 40, 45, 50, 60, 200};

void setup() {
//  pmc_mck_set_prescaler(PMC_PCK_PRES_CLK_64);
  pmc_disable_all_pck();
  pmc_disable_periph_clk(17);  // USART0
  pmc_disable_periph_clk(18);  // USART1
  pmc_disable_periph_clk(19);  // USART2
  pmc_disable_periph_clk(20);  // USART3 - using pins for GPIO
  pmc_disable_periph_clk(21);  // HSMCI (SD/MMC ctrl, N/C)
  pmc_disable_periph_clk(22);  // TWI/I2C bus 0 (i.MX6 controlling)
  pmc_disable_periph_clk(23);  // TWI/I2C bus 1
  pmc_disable_periph_clk(24);  // SPI0
  pmc_disable_periph_clk(25);  // SPI1
  pmc_disable_periph_clk(26);  // SSC (I2S digital audio, N/C)
  pmc_disable_periph_clk(38);  // DAC ctrl                  // no big savings
  pmc_disable_periph_clk(39);  // DMA ctrl

  pmc_disable_periph_clk(41);  // random number generator
  pmc_disable_periph_clk(42);  // ethernet MAC - N/C        // saves some mA - we never use this
  pmc_disable_periph_clk(43);  // CAN controller 0          // saves some mA - we never use this
  pmc_disable_periph_clk(44);  // CAN controller 1          // saves some mA - we never use this


  //start serial connection
  Serial.begin(9600);
  pinMode(pin_interrupt, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin_interrupt), get_timestamp, RISING);

  pinMode(pin_reset, INPUT_PULLUP); // pin to reset histogram
  pinMode(ledPin, OUTPUT); // indicator to see things are running when in standalone mode
  first_run = false;

  counter = 0; // counter to enable writing to flash

  // read histogram available in flash?
  byte* b = dueFlashStorage.readAddress(4);
  histogram_flash Flash_histogram_temp;
  memcpy(&Flash_histogram_temp, b, sizeof(Flash_histogram_temp));

  // check if flash data should be used for initialization
  bool use_flash_data = true;
  for (byte i = 0; i < 19; i++) {
    if (abs(Flash_histogram_temp.hist_buckets[i] - buckets[i]) > 0.01) {
      use_flash_data = false;
    }
  }

  if (use_flash_data) {
    memcpy(&Flash_histogram, b, sizeof(Flash_histogram_temp));
  }
  else {
    Flash_histogram.hist_total = 0;

    for (byte i = 0; i < 19; i++) {
      Flash_histogram.hist_buckets[i] = buckets[i];
      Flash_histogram.hist_counter[i] = 0;
    }

  }

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

  Serial.println("pin reset");
  Serial.println(hist_reset);

  // copy timestamp buffers to avoid updates during data-processing
  timestamps_index = timestamps_index_cp;
  for ( byte i = 0 ; i < 8 ; i++ ) {
    timestamps[ i ] = timestamps_cp[ i ];
  }

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

    }
  }
  else {
    v_wind_kph = 0;
  }




  Serial.println("wind speed:");
  Serial.println(v_wind_kph);

  // add windpseed to histogram
  for  (int i = 18; i >= 0; i--) {
    if (v_wind_kph >= Flash_histogram.hist_buckets[i]) {
      Flash_histogram.hist_counter[i] = Flash_histogram.hist_counter[i] + 1;
      break;
    }
  }
  Flash_histogram.hist_total = Flash_histogram.hist_total + 1;



  for (byte i = 0; i < 19; i++) {
    Serial.println(Flash_histogram.hist_counter[i]);
  }
  Serial.println(Flash_histogram.hist_total);
  Serial.println("----");

  // write every x minutes to flash
  counter++;
  if (counter == 600) {
    // write configuration struct to flash at adress 4
    byte b2[sizeof(Flash_histogram)]; // create byte array to store the struct
    memcpy(b2, &Flash_histogram, sizeof(Flash_histogram)); // copy the struct to the byte array
    dueFlashStorage.write(4, b2, sizeof(Flash_histogram)); // write byte array to flash
    Serial.println("write to flash done");
    counter = 0;
    digitalWrite(ledPin, HIGH);
  }

  digitalWrite(ledPin, LOW);


//  pmc_enable_backupmode();
  delay(time_delay); // give time_delay time to capture new data
  
}





void get_timestamp() {
  timestamp_newdata = true;
  timestamps_index_cp++;
  if (timestamps_index_cp > 7) {
    timestamps_index_cp = 0;
  }

  timestamps_cp[timestamps_index_cp] = micros();
}


