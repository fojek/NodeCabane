#include <SPI.h>
#include <LoRa.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "LowPower.h" // https://github.com/rocketscream/Low-Power

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(A0);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

uint8_t selected;

int counter = 0;

char zone = 'A'; // Changer selon la zone du capteur

int powerPin = 3;   // La pin qui alimente le MPX4250DP
int pressionPin = A7; // La sortie du MPX4250DP
int battPin = A2; // Lecture de la tension de la batterie

// Temps d'hibernation
class ModeHibernation {
public:
  int temps;
  float temperature;

  ModeHibernation(int t, float T) {
    this->temps = t;
    this->temperature = T;
  }
};

ModeHibernation * mOperation = new ModeHibernation(11, -1);  // 11*8s ~= 1.5min
ModeHibernation * mEconomie = new ModeHibernation(450, -8);  // 450*8s ~= 1h
ModeHibernation * mHibernation = new ModeHibernation(900, -30);  // 900*8s ~= 2h

void setup() {
  pinMode(powerPin, OUTPUT);
  
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Sender");
  LoRa.setPins(7, 6, 2);

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setTxPower(20);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
}

long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1125300L / result; // Back-calculate AVcc in mV
  return result;
}

float getBatt() {
  float vcc = readVcc()/1000;
  return (analogRead(battPin) * vcc / 1024);
}

float getTemp() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

int getPress() {
  // Cette pin est utilisée pour alimenter le MPX, pour 
  // Économiser de la batterie.
  digitalWrite(powerPin, HIGH);
  // Délai d'alimentation du MPX ... requis?
  delay(50);
  
  int pression = analogRead(pressionPin);
  digitalWrite(powerPin, LOW);

  // Retourne raw. Le mapping est fait dans Node-Red.
  return pression;
}

int getModeHibernation(float temperature, float batterie){
  

  return 10;
}

void loop() {
  
  // Valeurs
  float tempC = getTemp();
  float batt = getBatt();
  float pression = getPress();

  // Construit le paquet
  LoRa.beginPacket();
  LoRa.print(zone);
  LoRa.print(";");
  LoRa.print(getPress());
  LoRa.print(";");
  LoRa.print(tempC);
  LoRa.print(";");
  LoRa.print(getBatt());
  LoRa.print(";");
  LoRa.endPacket();



  for(int i=0; i<getModeHibernation(tempC, batt); ++i){
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}