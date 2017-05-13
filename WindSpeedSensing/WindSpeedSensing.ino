/*
  author: SG
  creation date: 8 May 2017
  hardware: arduino due

  read out wind speed sensor
  Eltako WS Windsensor

*/

const byte pin_led = 13;
const byte pin_interrupt = 32;

volatile long timestamps[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile byte timestamps_index = 0;

// copy to avoid updates during processing
volatile long timestamps_cp[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile byte timestamps_index_cp = 0;

int index_buffer = 0;
long time_delta[3] = {0, 0, 0};
long temporary;
float sensor_rps;
float v_wind_kph

void setup() {
  //start serial connection
  Serial.begin(9600);
  pinMode(pin_interrupt, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin_interrupt), get_timestamp, RISING);

  pinMode(pin_led, OUTPUT);

}

void loop() {
  //read the pushbutton value into a variable
  int sensorVal = digitalRead(pin_interrupt);
  //print out the value of the pushbutton

  // copy buffers
  timestamps_index = timestamps_index_cp;
  for ( byte i = 0 ; i < 8 ; i++ ) {
    timestamps[ i ] = timestamps_cp[ i ];
  }


  for (byte i = 0; i < 8; i = i + 1) {
    Serial.println(timestamps[i]);
  }

  Serial.println("--");
  // process data from circular buffer
  // two pulses per revolution are given
  // take median value from three delta_times
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

//    for (byte i = 0; i < 3; i = i + 1) {
//      Serial.println(time_delta[i]);
//    }


  // take second element out of buffer
  sensor_rps = 1000000 / (float)time_delta[1];
  v_wind_kph = 1.761 / (1 + sensor_rps) + 3.013 * sensor_rps;

  Serial.println(sensor_rps);
  Serial.println("----");

  //  if (sensorVal == HIGH) {
  //    digitalWrite(pin_led, LOW);
  //  } else {
  //    digitalWrite(pin_led, HIGH);
  //  }

  delay(100);
}

void get_timestamp() {
  timestamps_index_cp++;
  if (timestamps_index_cp > 7) {
    timestamps_index_cp = 0;
  }

  timestamps_cp[timestamps_index_cp] = micros();
}


