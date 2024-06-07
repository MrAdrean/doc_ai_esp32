
// #include "BluetoothSerial.h"
// #include "esp_bt_device.h"
// BluetoothSerial SerialBT;

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <MAX30102_PulseOximeter.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


// #include "MAX30100_PulseOximeter.h"

#define REPORTING_PERIOD_MS     1000

// Insert Firebase project API Key
#define API_KEY "AIzaSyArIwfOgTUddEClCoxwunNJRBR85v8fZdc"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://docai-e0120-default-rtdb.europe-west1.firebasedatabase.app/" 


// Define Firebase Data object.
FirebaseData fbdo;

// Define firebase authentication.
FirebaseAuth auth;

// Definee firebase configuration.
FirebaseConfig config;
bool signupOK = false;
float BPM, SpO2;

//======================================== Millis variable to send/store data to firebase database.
unsigned long sendDataPrevMillis = 0;
const long sendDataIntervalMillis = 1000; //--> Sends/stores data to firebase database every 10 seconds.
//======================================== 

/*Put your SSID & Password*/
const char* ssid = "7 lei";  // Enter SSID here
const char* password = "Daiobere";  //Enter Password here

uint32_t tsLastReport = 0;

// void printDeviceAddress()
// {
//     const uint8_t* point = esp_bt_dev_get_address();
    
//     for (int i = 0; i < 6; i++)
//     {
//        char str[3];
//        sprintf(str, "%02X", (int)point[i]);
//        Serial.print(str);
//        if (i < 5){
//        Serial.print(":");}
//     }
//     Serial.println();
// }
#include <Wire.h>
#include "MAX30105.h" //sparkfun MAX3010X library
#include "heartRate.h"
MAX30105 particleSensor;
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;

//#define MAX30105 //if you have Sparkfun's MAX30105 breakout board , try #define MAX30105 

#define USEFIFO
void setup()
{
  Serial.begin(115200);
  // SerialBT.begin("Pulse oximeter");
  // printDeviceAddress();
  Serial.println("Initializing...");

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30102 was not found. Please check wiring/power/solder jumper at MH-ET LIVE MAX30102 board. ");
    while (1);
  }

  //Setup to sense a nice looking saw tooth on the plotter
  byte ledBrightness = 0x7F; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode =2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  //Options: 1 = IR only, 2 = Red + IR on MH-ET LIVE MAX30102 board
  int sampleRate = 400; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 16384; //Options: 2048, 4096, 8192, 16384
  // Set up the wanted parameters
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings


  pinMode(19, OUTPUT);
  delay(100);
      //connect to your local wi-fi network
  WiFi.begin(ssid, password);
  Serial.println("Connecting to ");
  Serial.println(ssid);



  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", ""))
  {
    Serial.println("ok");
    signupOK = true;
  }
  else
  {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  Serial.println("---------------");

  // Assign the callback function for the long running token generation task.
  config.token_status_callback = tokenStatusCallback; //--> see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Firebase.RTDB.beginStream(&fbdo, "Vitals");
}



double avered = 0; double aveir = 0;
double sumirrms = 0;
double sumredrms = 0;
int i = 0;
int Num = 100;//calculate SpO2 by this sampling interval

double ESpO2 = 95.0;//initial value of estimated SpO2
double FSpO2 = 0.7; //filter factor for estimated SpO2
double frate = 0.95; //low pass filter for IR/red LED value to eliminate AC component
#define TIMETOBOOT 3000 // wait for this time(msec) to output SpO2
#define SCALE 88.0 //adjust to display heart beat and SpO2 in the same scale
#define MAX_SPO2 100.0
#define MIN_SPO2 80.0
#define SAMPLING 1//if you want to see heart beat more precisely , set SAMPLING to 1
#define FINGER_ON 30000 // if red signal is lower than this , it indicates your finger is not on the sensor
#define MINIMUM_SPO2 80.0
void loop()
{

  uint32_t ir, red , green;
  double fred, fir;
  double SpO2 = 0; //raw SpO2 before low pass filtered
  uint8_t ID = particleSensor.readPartID();

  #ifdef USEFIFO
  particleSensor.check(); //Check the sensor, read up to 3 samples

  while (particleSensor.available())
  {//do we have new data
    #ifdef MAX30105
      red = particleSensor.getFIFORed(); //Sparkfun's MAX30105
      ir = particleSensor.getFIFOIR();  //Sparkfun's MAX30105
    #else
      red = particleSensor.getFIFOIR(); //why getFOFOIR output Red data by MAX30102 on MH-ET LIVE breakout board
      ir = particleSensor.getFIFORed(); //why getFIFORed output IR data by MAX30102 on MH-ET LIVE breakout board
    #endif
      i++;
      fred = (double)red;
      fir = (double)ir;
      avered = avered * frate + (double)red * (1.0 - frate);//average red level by low pass filter
      aveir = aveir * frate + (double)ir * (1.0 - frate); //average IR level by low pass filter
      sumredrms += (fred - avered) * (fred - avered); //square sum of alternate component of red level
      sumirrms += (fir - aveir) * (fir - aveir);//square sum of alternate component of IR level
      if ((i % SAMPLING) == 0)
      {//slow down graph plotting speed for arduino Serial plotter by thin out
        if ( millis() > TIMETOBOOT) 
        {
    //        float ir_forGraph = (2.0 * fir - aveir) / aveir * SCALE;
    //        float red_forGraph = (2.0 * fred - avered) / avered * SCALE;
          float ir_forGraph = 2.0 * (fir - aveir) / aveir * SCALE + (MIN_SPO2 + MAX_SPO2) / 2.0;
          float red_forGraph = 2.0 * (fred - avered) / avered * SCALE + (MIN_SPO2 + MAX_SPO2) / 2.0;
          //trancation for Serial plotter's autoscaling
          if ( ir_forGraph > 100.0) ir_forGraph = 100.0;
          if ( ir_forGraph < 80.0) ir_forGraph = 80.0;
          if ( red_forGraph > 100.0 ) red_forGraph = 100.0;
          if ( red_forGraph < 80.0 ) red_forGraph = 80.0;
          //        Serial.print(red); Serial.print(","); Serial.print(ir);Serial.print(".");
          if (ir < FINGER_ON) ESpO2 = MINIMUM_SPO2; //indicator for finger detached
          Serial.print(ir_forGraph); // to display pulse wave at the same time with SpO2 data
          Serial.print(","); Serial.print(red_forGraph); // to display pulse wave at the same time with SpO2 data
          Serial.print(",");
          Serial.print(ESpO2); //low pass filtered SpO2
          Serial.print("bpm");
          Serial.println(beatAvg);
          // if(ESpO2<81){SerialBT.println("1");}
          // else{
          // SerialBT.print(String(ESpO2)+"a"+String(beatAvg));

            if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > sendDataIntervalMillis || sendDataPrevMillis == 0))
              {
                Firebase.RTDB.setFloat(&fbdo, "Users/-Nzn9Dz4xXWY_U8gX8ED/BPM", red_forGraph);
                Firebase.RTDB.setFloat(&fbdo, "Users/-Nzn9Dz4xXWY_U8gX8ED/SpO2", ESpO2);
                Firebase.RTDB.setFloat(&fbdo, "Users/-Nzn9Dz4xXWY_U8gX8ED/ID", ID);
              }
            
          delay(100);
        }
      }
      Serial.print("i ");
      Serial.println(i);
      if ((i % Num) == 0)
      {
        double R = (sqrt(sumredrms) / avered) / (sqrt(sumirrms) / aveir);
        // Serial.println(R);
        SpO2 = -23.3 * (R - 0.4) + 100; //http://ww1.microchip.com/downloads/jp/AppNotes/00001525B_JP.pdf
        ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * SpO2;//low pass filter
        //  Serial.print(SpO2);Serial.print(",");Serial.println(ESpO2);
        sumredrms = 0.0; sumirrms = 0.0; i = 0;
        break;
      }
      particleSensor.nextSample(); //We're finished with this sample so move to next sample
      //Serial.println(SpO2);
  }
  #else

    while (1)
    {//do we have new data
    #ifdef MAX30105
      red = particleSensor.getRed();  //Sparkfun's MAX30105
      ir = particleSensor.getIR();  //Sparkfun's MAX30105
    #else
      red = particleSensor.getIR(); //why getFOFOIR outputs Red data by MAX30102 on MH-ET LIVE breakout board
      ir = particleSensor.getRed(); //why getFIFORed outputs IR data by MAX30102 on MH-ET LIVE breakout board
    #endif
      i++;
      fred = (double)red;
      fir = (double)ir;
      avered = avered * frate + (double)red * (1.0 - frate);//average red level by low pass filter
      aveir = aveir * frate + (double)ir * (1.0 - frate); //average IR level by low pass filter
      sumredrms += (fred - avered) * (fred - avered); //square sum of alternate component of red level
      sumirrms += (fir - aveir) * (fir - aveir);//square sum of alternate component of IR level
      if ((i % SAMPLING) == 0)
      {//slow down graph plotting speed for arduino IDE toos menu by thin out
        //#if 0
        if ( millis() > TIMETOBOOT)
        {
          float ir_forGraph = (2.0 * fir - aveir) / aveir * SCALE;
          float red_forGraph = (2.0 * fred - avered) / avered * SCALE;
          //trancation for Serial plotter's autoscaling
          if ( ir_forGraph > 100.0) ir_forGraph = 100.0;
          if ( ir_forGraph < 80.0) ir_forGraph = 80.0;
          if ( red_forGraph > 100.0 ) red_forGraph = 100.0;
          if ( red_forGraph < 80.0 ) red_forGraph = 80.0;
          //        Serial.print(red); Serial.print(","); Serial.print(ir);Serial.print(".");
          if (ir < FINGER_ON) ESpO2 = MINIMUM_SPO2; //indicator for finger detached
          Serial.print((2.0 * fir - aveir) / aveir * SCALE); // to display pulse wave at the same time with SpO2 data
          Serial.print(","); Serial.print((2.0 * fred - avered) / avered * SCALE); // to display pulse wave at the same time with SpO2 data
          Serial.print("ESpO2 "); Serial.print(ESpO2); //low pass filtered SpO2
          //#endif
        }
      }

      Serial.println("i ");
      Serial.print(i);
      if ((i % Num) == 0)
      {
        double R = (sqrt(sumredrms) / avered) / (sqrt(sumirrms) / aveir);
        // Serial.println(R);
        SpO2 = -23.3 * (R - 0.4) + 100; //http://ww1.microchip.com/downloads/jp/AppNotes/00001525B_JP.pdf
        ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * SpO2;
        //  Serial.print(SpO2);Serial.print(",");Serial.println(ESpO2);
        sumredrms = 0.0; sumirrms = 0.0; i = 0;
        Serial.print("BREAK");
        break;
      }
      particleSensor.nextSample(); //We're finished with this sample so move to next sample
      //Serial.println(SpO2);
    }
  #endif
  long irValue = particleSensor.getIR();

  Serial.println("checkForBeat");
  Serial.print(checkForBeat(irValue));
    if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    lastBeat = millis();
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      Serial.print("RATE_SIZE");
      Serial.print(RATE_SIZE);
      Serial.print("rateSpot");
      Serial.print(rateSpot);

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
      {
        Serial.print("if for ");
        Serial.print(x);
        beatAvg += rates[x];
      }
        
      beatAvg /= RATE_SIZE;
    }
  }










}
