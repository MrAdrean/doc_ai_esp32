
//MAX30100 ESP32 WebServer
#include <WiFi.h>
// #include <WebServer.h>
#include <Wire.h>
#include <MAX30102_PulseOximeter.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
// #include "BluetoothSerial.h"
// #include "esp_bt_device.h"


// BluetoothSerial SerialBT;

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
float BPM, SpO2, BPMavg, O2avg;

//======================================== Millis variable to send/store data to firebase database.
unsigned long sendDataPrevMillis = 0;
const long sendDataIntervalMillis = 1000; //--> Sends/stores data to firebase database every 10 seconds.
//======================================== 

/*Put your SSID & Password*/
const char* ssid = "Adrian's iPhone";  // Enter SSID here
const char* password = "adrianeparola";  //Enter Password here

PulseOximeter pox;
VEGA_MAX30102 hrm;
uint32_t tsLastReport = 0;

// WebServer server(80);

void onBeatDetected()
{
  Serial.println("Beat Detected!");
}

void setup()
{

  Serial.begin(115200);
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

  // server.on("/", handle_OnConnect);
  // server.onNotFound(handle_NotFound);

  // server.begin();
  Serial.println("HTTP server started");

  Serial.print("Initializing pulse oximeter..");

  Serial.println(hrm.getPartId());
  Serial.println(EXPECTED_PART_ID);

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
  // Firebase.RTDB.beginStream(&fbdo_2, "Vitals/SpO2");

  if (!pox.begin())
  {
    Serial.println("FAILED");
    for (;;);
  }
  else
  {
    Serial.println("SUCCESS");

    pox.setOnBeatDetectedCallback(onBeatDetected);
  }

  pox.setIRLedCurrent(RED_LED_CURRENT_START);
  
  }


  // Register a callback for the beat detection


void loop()
{
    pox.update();
    Serial.print("BPM: ");
    Serial.println(BPM);

    Serial.print("SpO2: ");
    Serial.print(SpO2);
    Serial.println("%");

    Serial.println("*********************************");
    Serial.println();

    sendDataPrevMillis = millis();

  // SerialBT.print(String(SpO2)+"a"+String(BPM));
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > sendDataIntervalMillis || sendDataPrevMillis == 0))
  {

 

    BPM = pox.getHeartRate();
    SpO2 = pox.getSpO2();
    Firebase.RTDB.setFloat(&fbdo, "Vitals/BPM", BPM);
    Firebase.RTDB.setFloat(&fbdo, "Vitals/SpO2", SpO2);
  //   if (Firebase.RTDB.setFloat(&fbdo, "Vitals/Data", SpO2))
  //   {
  //     Serial.println("PASSED");
  //     Serial.println("PATH: " + fbdo.dataPath());
  //     Serial.println("TYPE: " + fbdo.dataType());
  //   }
  //   else
  //   {
  //     Serial.println("FAILED");
  //     Serial.println("REASON: " + fbdo.errorReason());
  //   }
    
  //   if (Firebase.RTDB.setFloat(&fbdo, "Vitals/SpO2", SpO2))
  //   {
  //     Serial.println("PASSED");
  //     Serial.println("PATH: " + fbdo.dataPath());
  //     Serial.println("TYPE: " + fbdo.dataType());
  //   }
  //   else
  //   {
  //     Serial.println("FAILED");
  //     Serial.println("REASON: " + fbdo.errorReason());
  //   }
  }

}

