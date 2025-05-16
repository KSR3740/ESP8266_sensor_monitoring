#include<Arduino.h>
#include<ArduinoJson.h>
#include<BH1750.h>
#include<OneWire.h>
#include<Wire.h>
#include<DallasTemperature.h>
#include<ESP8266WiFi.h>
#include<WiFiClientSecure.h>
#include<ESP8266HTTPClient.h>

#define AOUT_PIN A0  // The ESP8266 pin ADC0 that connects to AOUT pin of moisture sensor
#define DOOR_SENSOR_PIN D5
#define ONE_WIRE_BUS D6  // GPIO4
#define BUZZER_PIN D7

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const char* ssid = "BDGTRONIX1"; //--> Your wifi name or SSID.
const char* password = "bdgtronix@321"; //--> Your wifi password.

const char* host = "script.google.com";
const int httpsPort = 443;

WiFiClientSecure client;
String GAS_ID = "AKfycbwMTbtZt9PjBezCVVRATBHtI-47Ynu20zAeVRSOWNu3hYAiY5BzzgeDjYFrzCJI7Yq-";//spreadsheet script ID

BH1750 lightMeter;

unsigned long startime, currenttime = 0; 
unsigned long ten_min_delay = 10*1000;//10 minute delay

struct data{
  char INT_flag;//interrupt flag for door status
  int Doorstate;
  float value;//soil moisture value
  float lux;//luminosity
  float temp;//temperature
  char door_status[10];//string to store door status as "OPEN" or "CLOSED"
}d;

struct SheetValues//stucture to read values from gsheet
{
  int A2;//TLT
  int B2;//BTO
  bool success;
}values;


void upload_data()//func to upload data to google sheet
{
  String soil_moisture = String(d.value);// + "";
  String luminosity = String(d.lux);// + " lx";
  String temp = String(d.temp);// + " °C";
  String door_state = String(d.door_status);// + "";

  Serial.println("==========");
  Serial.print("connecting to ");
  Serial.println(host);

  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  String string_temperature =  String(temp);
  String string_moisture =  String(soil_moisture); 
  String string_lx =  String(luminosity);
  String string_door =  String(door_state);
  String url = "/macros/s/" + GAS_ID + "/exec?temperature=" + string_temperature + "&soil_moisture=" + string_moisture + "&luminosity=" + string_lx + "&door_state=" + string_door;
  Serial.print("requesting URL: ");
  Serial.println(url);
 
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");
 
  Serial.println("request sent");
  //----------------------------------------
 
  //----------------------------------------Checking whether the data was sent successfully or not
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  // if (line.startsWith("{\"state\":\"success\"")) {
  //   Serial.println("esp8266/Arduino CI successfull!");
  // } else {
  //   Serial.println("esp8266/Arduino CI has failed");
  // }

  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("==========");
}

void display_data()//display collected data to serial monitor
{
  Serial.println(d.door_status);
  Serial.print("Moisture: ");
  Serial.println(d.value);
  Serial.print("Light: ");
  Serial.print(d.lux);
  Serial.println(" lx");
  Serial.print("Temperature: ");
  Serial.print(d.temp);
  Serial.println(" °C");
}

void buzzer()
{
  int buzzer_start = values.B2;
  if(d.temp >= values.A2)
  {
    digitalWrite(BUZZER_PIN,HIGH);
    delay(buzzer_start*1000);
    digitalWrite(BUZZER_PIN,LOW);
  }
}

void collect_data()//collect data from all sensors
{
  d.value = analogRead(AOUT_PIN); // read the analog value from moisture sensor
  d.Doorstate = digitalRead(DOOR_SENSOR_PIN); // read door state
  d.lux = lightMeter.readLightLevel();
  sensors.requestTemperatures();
  d.temp = sensors.getTempCByIndex(0);

  if(d.Doorstate == LOW)
  {
    strcpy(d.door_status,"CLOSED");
  }
  else if(d.Doorstate == HIGH)
  {
    strcpy(d.door_status,"OPEN");
  }
  buzzer();
}

void IRAM_ATTR isr()//call isr when door state changes
{
  d.INT_flag = 1;
}

void setup() {
  Serial.begin(9600);
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  attachInterrupt(DOOR_SENSOR_PIN, isr, CHANGE);
  Wire.begin(D2, D1); // SDA, SCL
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23);
  sensors.begin();
  WiFi.begin(ssid, password); //--> Connect to your WiFi router
  Serial.println("");

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("Successfully connected to : ");
  Serial.println(ssid);
  client.setInsecure();
}

SheetValues read_gsheet()
{
  SheetValues result = {0, 0, false};
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return result;
  }

  String url = "https://script.google.com/macros/s/" + GAS_ID + "/exec";
  Serial.println("Requesting URL: " + url);
  
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, url);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Received payload:");
    Serial.println(payload);

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      result.A2 = doc["A2"];
      result.B2 = doc["B2"];
      result.success = true;
    } else {
      Serial.print("JSON error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
  }
  
  http.end();
  return result;
}


//FUNCTION FOR ASSIGNING VALUE TO VARIABLES RECEIVED FOR GOOGLE_SHEET
void read()
{
  values = read_gsheet();
  
  if (values.success) 
  {
    Serial.print("A2: "); Serial.println(values.A2);
    Serial.print("B2: "); Serial.println(values.B2);
  } else 
  {
    Serial.println("Failed to read from Google Sheet");
  }

}

void loop() 
{
  startime = millis();
  if(startime - currenttime >= ten_min_delay)//check if 10 minutes has passed
  {
    //call function to read from gsheet
    read();
    collect_data();
    display_data();
    upload_data();
    currenttime = startime;
  }

  if(d.INT_flag)
  {
    //call function to read from gsheet
    read();
    collect_data();
    display_data();
    upload_data();
    d.INT_flag = 0;
  }
}