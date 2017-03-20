/*
    PUBSUB MQTT library used in home automation - esp8266 nodemcu

    io.adafruit.com broker, but using the MQTT PubSub client library. 
    
    17.12.2016: Ricardo Timmermann inspired by Andreas Spiess and of course, Ivan Grokhotkov.
    
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>

// == Assign Arduino Friendly Names to GPIO pins =========================

#define D0  16
#define D1  5       // I2C Bus SCL (clock)
#define D2  4       // I2C Bus SDA (data)
#define D3  0
#define D4  2       // Same as "LED_BUILTIN", but inverted logic
#define D5  14      // SPI Bus SCK (clock)
#define D6  12      // SPI Bus MISO 
#define D7  13      // SPI Bus MOSI
#define D8  15      // SPI Bus SS (CS)
#define D9  3       // RX0 (Serial console)
#define D10 1       // TX0 (Serial console)

// == WiFi Access Point ==================================================

#define WLAN_SSID           "Quilaco"
#define WLAN_PASS           ""

// == MQTT Broker ========================================================

#define MQTT_SERVER         "io.adafruit.com"
#define MQTT_SERVERPORT     1883
#define MQTT_USERNAME       "***"
#define MQTT_KEY            "***"
#define MQTT_FEED           "gosouth/feeds/"

#define T_LUMINOSITY    "battery"
#define T_LIGHT         "light"
#define T_HUPE          "hupe"
#define T_KUECHE        "IR-kueche"
#define T_GALPON        "galpon"
#define T_BDOOR         "bdoor"

// == HW GPIO defines =====================================================

#define LED     D0
#define LIGHT   D1
#define HUPE    D2

#define KUECHE  D5
#define GALPON  D6
#define BDOOR   D7

#define ADC     0

// == globals =====================================================

unsigned long entry;

int LedStatus, prevLedStatus = -1;
int KuecheStatus, prevKuecheStatus = -1;
int GalponStatus, prevGalponStatus = -1;
int luminosity, prevLumiosity = -1;
int backDoor, prevBackDoor = -1;

// Callback function header
void callback(char* topic, byte* payload, unsigned int length);

WiFiClient ethClient;
PubSubClient mqttClient( ethClient );

// == watchDog settings ====================================================

Ticker secondTick;
volatile int watchdogCount = 0;

void ISRwatchdog() 
{
    ++watchdogCount; 
    if( watchdogCount == 15 ) {
        Serial.printf("\n\n== Watchdog timeout ==\n\n");
        ESP.reset();
    }
}

// == blink led by MQTT activity ===========================================

void blinkLed()
{
    digitalWrite(LED, LOW);
    delay(100);
    digitalWrite(LED, HIGH);
}

// == the setup =============================================================

void setup() 
{
    // == define GPIO mode & serial
    pinMode(LED, OUTPUT);
    pinMode(LIGHT, OUTPUT);
    pinMode(HUPE, OUTPUT);                  // Hupe = siren bell
        
    pinMode(KUECHE, INPUT_PULLUP);          // IR-sensor, signals me if Kids are close to the fridge 
    pinMode(GALPON, INPUT_PULLUP);          // This is a magned switch, signals if the dorr is open.
    pinMode(BDOOR, INPUT_PULLUP);           // now ised only to test ISR.
        
    Serial.begin( 115200 );
    delay(100);

    // == ISR watchdog
    secondTick.attach( 1, ISRwatchdog );

    // == WLan connection
    Serial.printf("\n\nConnecting to: %s ", WLAN_SSID);
    WiFi.begin( WLAN_SSID, WLAN_PASS );
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        }
    Serial.println();
 
    Serial.print("IP address: " );
    Serial.println( WiFi.localIP() );
    WiFi.printDiag(Serial);
    Serial.println();

    // == MQTT client
    mqttClient.setServer( MQTT_SERVER, MQTT_SERVERPORT );
    mqttClient.setCallback(callback);
}


// === the famous main loop ================================================

void loop() 
{

    watchdogCount = 0;      // reset watchdog to signal keepalife
    
    yield();

    // == connect to mqtt ========================
    
    if (!mqttClient.connected()) {
        
        Serial.println("Attempting MQTT connection...");
        
        if (mqttClient.connect( "", MQTT_USERNAME, MQTT_KEY)) {
            Serial.printf("connected\n\n");
            
            // ... and resubscribe
            mqttClient.subscribe( MQTT_FEED T_LIGHT, 1);
            mqttClient.subscribe( MQTT_FEED T_HUPE, 1);

            mqttClient.publish( MQTT_FEED T_HUPE, "OFF");
            } 
            
        else {
            Serial.printf("failed, rc = %s try again in 5 seconds\n", mqttClient.state() );
             // Wait 5 seconds before retrying
            delay(5000);
            }
        }

    // == read analog input each 5 secs ================
    
    if( millis() - entry > 5000 ) {
        entry = millis();
        luminosity = analogRead(A0);
        }

    // == publish fotocell luminosity, in future solar battery status ==============
    
    if (mqttClient.connected() && prevLumiosity != luminosity) {

        blinkLed();
        char buf[16];
        itoa(luminosity,buf,10);
        
        mqttClient.publish( MQTT_FEED T_LUMINOSITY, buf );
        prevLumiosity = luminosity;
        delay(500);
        }

   // == publish current light switch status (a kind of QoS), I'm using same suscribed feed.
    
    if (mqttClient.connected() && prevLedStatus != LedStatus) {
        
        blinkLed();
        Serial.println("< Publish current Light Status");
        prevLedStatus = LedStatus;
        
        if( LedStatus==0 ) 
            mqttClient.publish( MQTT_FEED T_LIGHT, "OFF");
        else 
            mqttClient.publish( MQTT_FEED T_LIGHT, "ON");
        }

    // == publish current Kueche status 

    KuecheStatus = digitalRead( KUECHE );
    if (mqttClient.connected() && prevKuecheStatus != KuecheStatus) {
        
        blinkLed();
        Serial.println("< Publish Kueche Status");
        prevKuecheStatus = KuecheStatus;
        
        if( KuecheStatus==1 ) 
            mqttClient.publish( MQTT_FEED T_KUECHE, "Kueche");
        }

    // == publish current Galpon status 

    GalponStatus = digitalRead( GALPON );
    if (mqttClient.connected() && prevGalponStatus != GalponStatus) {
        
        blinkLed();
        Serial.println("< Publish Galpon Status");
        prevGalponStatus = GalponStatus;
        
        if( GalponStatus==0 ) 
            mqttClient.publish( MQTT_FEED T_GALPON, "Galpon OPEN");
        else 
            mqttClient.publish( MQTT_FEED T_GALPON, "Galpon CLOSE");
            
        }
        
     
    // == reset device testing IST. When GPIO BDOOR is high it should enter ISR routine.
    // After uploading the code you should HW reset the ESP8266, so it works. There are some
    // issues described by:
    //
    // https://github.com/esp8266/Arduino/issues/1017
    
    backDoor = digitalRead( BDOOR );

    if( backDoor==1 ) {
        for( int k=0; k<50; ++k) {
            Serial.printf("Reset mode %d\n", k );
            delay(1000);
        }
    }

    mqttClient.loop();
}

// == handle mqtt messages / subscriptions =======================================

void callback( char* topic, byte* data, unsigned int length ) 
{

    blinkLed();
    
    String str( (char *)data );
    Serial.printf( "> %s : ", topic );
    Serial.println( str.substring( 0, length) );

    // == light switch ====================
    if( strstr( topic, T_LIGHT ) ) {
           
        if (data[1] == 'F')  {
            LedStatus = 0;
            digitalWrite(LIGHT, LOW);
            }
             
        else {
            LedStatus = 1;
            digitalWrite(LIGHT, HIGH);
            }
    }

    // == Hupe (Sound Alarm for 4 secs) ====================
    if( strstr( topic, T_HUPE ) ) {
          
        if (data[1] == 'N')  {
            digitalWrite(HUPE, HIGH);
            delay(4000);
            digitalWrite(HUPE, LOW);
            
            mqttClient.publish( MQTT_FEED T_HUPE, "OFF");
            }
    }
}




