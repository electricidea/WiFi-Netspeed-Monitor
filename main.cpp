/******************************************************************************
 * Netspeed WIFI monitoring
 * ESP8266 / ESP32 routines for monitoring the performance of an WIFI Network
 * Hague Nusseck @ electricidea
 * v2.6 14 April 2020
 * https://github.com/electricidea/WiFi-Netspeed-Monitor
 * 
 * Thanks to:
 * 3zuli at instructables for inspiring me to this project
 * https://www.instructables.com/id/ESP8266-Internet-Alarm/
 *
 * 
 * 
 * Distributed as-is; no warranty is given.
 ******************************************************************************/

#include <Arduino.h>
// for ESP32: include Wifi.h
//#include "WiFi.h"
// for ESP8266: include ESP8266WiFi.h
#include <ESP8266WiFi.h>

#include <WiFiClientSecure.h>
#include <TimeLib.h>
// pio lib install "Time"
// lib_deps = Time

// pin number of the LED on the board
#define LED_BUILTIN 16
// LED code:
//  short ON-blinks every second: Idle loop = waiting one minute
//  On with short OFF-blinks: Performing speed test
//  permanently on: Writing data to circusofthings.com
#define LED_ON LOW
#define LED_OFF HIGH

// verbose level:
// TRUE = lot of data
// FALSE = only time and speed data (table format)
#define verbose_output false

// WiFi network configuration:
const char* ssid     = "Your-WiFi";
const char* password = "totally-secret";


// circusofthings.com configuration
// general settings to connect
const char* Circus_hostname = "circusofthings.com";
const char* Circus_token = "abcdefghijklmnopq-check-your-account-abcdefghijklmnopq";
// special setting for the signal
const char* Circus_key = "123456789";

// time in milliseconds
unsigned long startMillis;
unsigned long lastMillis;
// Buffer values for Seconds and minutes
// used to let the LED blink everey second
// and perform a test every minute
uint8_t last_second;
int minute_count;

// function forward declaration
void scan_WIFI();
boolean connect_Wifi();
int server_get();
int Circus_write(double value);
int Circus_write(const char* Circus_key, double value);


// =============================================================
void setup() {
  // init serial connection
  Serial.begin(115200);
  Serial.println("");
  Serial.println("-----------------------");
  Serial.println("--      NetSpeed     --");
  Serial.println("-- v2.6 / 13.04.2020 --");
  Serial.println("-----------------------");
  // init on board LED (red)
  pinMode (LED_BUILTIN, OUTPUT);
  // turn the LED on for one second)
  digitalWrite(LED_BUILTIN, LED_ON);
  delay(1000);
  digitalWrite(LED_BUILTIN, LED_OFF);

  // Set WiFi to station mode and disconnect
  // from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(250);
  // scan for available APs
  scan_WIFI();
  delay(250);
  // connect to the configured AP
  connect_Wifi();
  // If the connection attempt was not successful, 
  // a new attempt is started in the loop().
  delay(250);
  last_second = second(now());
  minute_count = 0;
  Serial.println("Setup done");
  // header line for data logging if not in verbose mode
  if(!verbose_output)
    Serial.println("time speed");
  // turn the LED on for one second)
  digitalWrite(LED_BUILTIN, LED_ON);
  delay(1000);
  digitalWrite(LED_BUILTIN, LED_OFF);
}


// =============================================================
void loop() {
  // get actual time
  time_t t=now();

  // call every second:
  if (second(t) != last_second) {
    // let the LED blink every second
    last_second = second(t);
    digitalWrite(LED_BUILTIN, LED_ON);
    delay(100);
    digitalWrite(LED_BUILTIN, LED_OFF);
    // increase to count 60 seconds
    minute_count++;
  }
  // call every 60 seconds:
  if (minute_count >= 60) {
    // check if WIFI is still connected
    // if the WIFI ist not connected (anymore)
    // a reconnect is triggert
    wl_status_t wifi_Status = WiFi.status();
    if(wifi_Status != WL_CONNECTED){
        // reconnect if the connection get lost
        Serial.println("[ERR] Lost WiFi connection, reconnecting...");
        if(connect_Wifi()){
          Serial.println("[OK] WiFi reconnected");
        } else {
          Serial.println("[ERR] unable to reconnect");
        }
    }
    // check if WIFI is connected
    // needed because of the above mentioned reconnection attempt
    wifi_Status = WiFi.status();
    if(wifi_Status == WL_CONNECTED){
      if(verbose_output) {
        Serial.print("[OK] WiFi connected / "); 
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.println("-----------------------");
      }
      // remeber the start time of the test
      startMillis = millis();
      int test_time;
      double test_speed;
      // change the LED code to show that the test is running
      digitalWrite(LED_BUILTIN, LED_ON);
      for(int n=1; n<=10; n++){
        lastMillis = millis();
        // call the http-GET and measure the required time
        server_get();
        test_time = millis()-lastMillis;
        if(verbose_output) Serial.printf("%i ==> %i milliseconds\n",n,test_time);
        // short LED OFF-blink to indicate the get request
        digitalWrite(LED_BUILTIN, LED_OFF);
        delay(100);
        digitalWrite(LED_BUILTIN, LED_ON);
      }
      // calculate the overall time for 10 get requests
      // note; 10x delay(100)=1000
      test_time = (millis()-startMillis)-1000;
      // Calculate the speed by dividing the number of get requests
      // by the time required.
      // speed unit = requests per second
      test_speed = 10.0 / double(test_time/1000.0);
      if(verbose_output) {
        Serial.println("-----------------------");
        Serial.printf("==> %i milliseconds total\n",test_time);
        Serial.printf("==> netspeed: %4.4f\n",test_speed);
      } else {
        // Simple table output for later analysis
        Serial.printf("%4.2f %4.2f\n",double((millis()/1000)), test_speed);
      }
      // Write the measured speed to circusofthings.com
      Circus_write(test_speed);
      digitalWrite(LED_BUILTIN, LED_OFF);
    }
    // prepare the next call in one minute
    t=now();
    last_second = second(t);
    minute_count = 0;
  }
}

// =============================================================
// scan_WIFI()  
// Scan for available Wifi networks 
// print all APs als simple list to the serial port
// ============================================================= 
void scan_WIFI() {
      Serial.println("WIFI scan ...");
      // WiFi.scanNetworks returns the number of networks found
      int n = WiFi.scanNetworks();
      if (n == 0) {
          Serial.println("[ERR] no networks found");
      } else {
          Serial.printf("[OK] %i networks found:\n",n);
          for (int i = 0; i < n; ++i) {
              // Print SSID for each network found
              Serial.printf("  %i: ",i+1);
              Serial.println(WiFi.SSID(i));
              delay(10);
          }
      }
      Serial.println("...");
}


// =============================================================
// connect_Wifi()
// connect to configured Wifi Access point
// returns true if the connection was successful otherwise false
// =============================================================
boolean connect_Wifi(){
  // Establish connection to the specified network until success.
  // Important to disconnect in case that there is a valid connection
  WiFi.disconnect();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  //Start connecting (done by the ESP in the background)
  WiFi.begin(ssid, password);
  // read wifi Status
  wl_status_t wifi_Status = WiFi.status();
  int n_trials = 0;
  // loop while Wifi is not connected
  // run only for 20 trials.
  while (wifi_Status != WL_CONNECTED && n_trials < 20) {
    // Check periodicaly the connection status using WiFi.status()
    // Keep checking until ESP has successfuly connected
    wifi_Status = WiFi.status();
    n_trials++;
    switch(wifi_Status){
      case WL_NO_SSID_AVAIL:
          Serial.println("[ERR] WIFI SSID not available");
          break;
      case WL_CONNECT_FAILED:
          Serial.println("[ERR] WIFI Connection failed");
          break;
      case WL_CONNECTION_LOST:
          Serial.println("[ERR] WIFI Connection lost");
          break;
      case WL_DISCONNECTED:
          Serial.println("[ERR] WiFi disconnected");
          break;
      case WL_IDLE_STATUS:
          Serial.println("[ERR] WiFi idle status");
          break;
      case WL_SCAN_COMPLETED:
          Serial.println("[OK] WiFi scan completed");
          break;
      case WL_CONNECTED:
          Serial.println("[OK] WiFi connected");
          break;
      default:
          Serial.println("[ERR] WIFI unknown Status");
          break;
    }
    delay(500);
  }
  if(wifi_Status == WL_CONNECTED){
    // if connected
    Serial.println("[OK] WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    // if not connected
    Serial.println("[ERR] unable to connect Wifi");
    return false;
  }
}

// =============================================================
// server_get()
// Call a GET request to httpbin.org. 
// Wait until a response is available but the response itself is ignored
// return values is the number of milliseconds until a response was available
// =============================================================
int server_get(){
  // Use WiFiClient class to create TCP connection
  WiFiClient client;
  // use httpbin.org to test the connection
  // httpbin returns a very small amount of data and is highly reliable
  const char* test_hostname = "httpbin.org";
  const int httpPort = 80;
  // Attempt to connect to httpbin.org:80 (http)
  if (!client.connect(test_hostname, httpPort)) {
    // If we can't connect, we obviously don't have internet
    Serial.println("[ERR] GET-Server Connection failed!!!");
    return 0;
  } else {
    String url = "/get";
    // We need to manually create the HTTP GET message
    // client.print() will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + test_hostname + "\r\n" +
                 "Connection: close\r\n\r\n");
    // Wait until a response is available
    // Wait up to 5 seconds for server to respond
    // then skip waiting
    int n_trials = 500;
    while((!client.available()) && (n_trials > 0)){
      n_trials--;
      delay(10);
    }
    // we only need the time until the response is available
    // therefore, we skp reading the response. 
    // for referece, this would be the code if one need the data:
    /*
    // Read all the lines of the reply from server and print them to Serial
    int n_lines = 0;
    while(client.available()){
      String line = client.readStringUntil('\r');
      n_lines++;
      //Serial.print(line);
    }
    Serial.printf("[OK] %i lines received\n",n_lines);
    return n_lines;
    */
   // return the milliseconds until a response was available
    return n_trials*10;
  }
}


// =============================================================
// Circus_write(const char* Circus_key, double value)
// send value to circusofthings.com website
// the hostname and token need to be configured globally
// the key is the id of the signal, the values should be added to 
// The function returns the number of received lines
// =============================================================
int Circus_write(const char* Circus_key, double value) {
  // Use WiFiClientSecure class to create TCP connection
  WiFiClientSecure client;
  const int httpsPort = 443;
  if(verbose_output) Serial.printf("--> connect to: %s:%i\r\n",Circus_hostname,httpsPort);
  int n_lines = 0;
  // check the connection
  if (!client.connect(Circus_hostname, httpsPort)) {
    Serial.println("[ERR] Circus Connection failed!!!");
    return 0;
  } else {
    // convert the value into a char-Array
    char value_char[15];
    dtostrf(value,1,4,value_char);
    // create the URI for the request
    char url_char[250];
    sprintf_P(url_char, PSTR("/WriteValue?Key=%s&Value=%s&Token=%s\r\n"), Circus_key, value_char, Circus_token);
    String url_str = url_char;
    if(verbose_output) {
      Serial.print("Requesting URL: ");
      Serial.println(Circus_hostname+url_str);
    }
    // We need to manually create the HTTP GET message
    // client.print() will send the request to the server
    client.print(String("GET ") + url_str + " HTTP/1.1\r\n" +
                 "Host: " + Circus_hostname + "\r\n" +
                 "Connection: close\r\n\r\n");
    // Wait until a response is available
    // But only a number of times
    // Wait up to 5 seconds for server to respond
    // then skip waiting
    int n_trials = 500;
    while(!client.available() && n_trials > 0){
      n_trials--;
      delay(10);
    }
    // Read all the lines of the reply from server
    // force timeout to 1000ms (ESP8266 = 5000ms default value)
    client.setTimeout(1000);
    while(client.available()){
      String line = client.readStringUntil('\r');
      n_lines++;
      if(verbose_output) Serial.print(line);
    }
    if(verbose_output) Serial.printf("\n[OK] %i lines received\n",n_lines);
  }
  return n_lines;
}
