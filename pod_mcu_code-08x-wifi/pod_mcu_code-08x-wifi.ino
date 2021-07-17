

#include <WiFi.h>
#include <HTTPClient.h>


#define LILYGO_WATCH_2019_WITH_TOUCH 
#include <LilyGoWatch.h>
TTGOClass *watch;
TFT_eSPI *tft;
BMA *sensor;


#include <Wire.h>

#include "SparkFun_BNO080_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_BNO080
BNO080 myIMU;



const char* ssid = "NETGEAR31";
const char* password = "fluffywind2904";

//Your Domain name with URL path or IP address with path
const char* serverName = "http://13.56.213.25:1234/sendData";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 10;

String response;

void setup() {
  Serial.begin(115200);

      // Get TTGOClass instance
    watch = TTGOClass::getWatch();

    // Initialize the hardware, the BMA423 sensor has been initialized internally
    watch->begin();

    // Turn on the backlight
    watch->openBL();

    //Receive objects for easy writing
    tft = watch->tft;

    // Some display settings
    tft->setTextColor(random(0xFFFF));
    tft->drawString("BNO08x Quaternions",  5, 50, 4);
    tft->setTextFont(4);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);


      WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
 
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");


  delay(100); //  Wait for BNO to boot
  // Start i2c and BNO080
  Wire.flush();   // Reset I2C
  myIMU.begin(BNO080_DEFAULT_ADDRESS, Wire);
  Wire.begin(25, 26);

   if (myIMU.begin() == false)
  {
    Serial.println("BNO080 not detected at default I2C address. Check your jumpers and the hookup guide. Freezing...");
    while (1);
  }

  Wire.setClock(400000); //Increase I2C data rate to 400kHz

  myIMU.enableRotationVector(50); //Send data update every 50ms

  Serial.println(F("Rotation vector enabled"));
  Serial.println(F("Output in form i, j, k, real, accuracy"));
  
}

void loop() {
 if ((millis() - lastTime) > timerDelay) {
    //Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED){

  if (myIMU.dataAvailable() == true)
  {
    float quatI = myIMU.getQuatI();
    float quatJ = myIMU.getQuatJ();
    float quatK = myIMU.getQuatK();
    float quatReal = myIMU.getQuatReal();
    float quatRadianAccuracy = myIMU.getQuatRadianAccuracy();

    Serial.print(quatI, 2);
    Serial.print(F(" "));
    Serial.print(quatK, 2);
    Serial.print(F(" "));
    Serial.print(quatJ, 2);
    Serial.print(F(" "));
    Serial.println(quatReal, 2);

     tft->fillRect(98, 100, 70, 85, TFT_BLACK);
      tft->setCursor(80, 100);
      tft->print("X:"); tft->println(quatI);
      tft->setCursor(80, 130);
      tft->print("Y:"); tft->println(quatJ);
      tft->setCursor(80, 160);
      tft->print("Z:"); tft->println(quatK);
      tft->setCursor(80, 190);
      tft->print("W:"); tft->println(quatReal);
  }
  
      
     
     // String url = String(serverName) + "?x=" + x + "&y=" + y + "&z=" + z; 
     // Serial.println(url);       
     // response = httpGETRequest(url.c_str());
     // Serial.println(response);


      


    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}

String httpGETRequest(const char* serverName) {
  HTTPClient http;
    
  // Your IP address with path or Domain name with URL path 
  http.begin(serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
