#include <SPI.h>
#include <LoRa.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "LowPower.h" // https://github.com/rocketscream/Low-Power

// Creation des objets OneWire et sensors
OneWire oneWire(A0);
DallasTemperature sensors(&oneWire);

// Configuration
char zone = 'B'; // Changer selon la zone du capteur
int powerPin = 3;   // La pin qui alimente le MPX4250DP
int pressionPin = A7; // La sortie du MPX4250DP
int battPin = A2; // Lecture de la tension de la batterie

// Temps d'hibernation
const int MODE_HIBERNATION = 900; // 4h = 900 x 8s
const int MODE_ECONOMIE = 450; // 2h = 450 x 8s
const int MODE_STANDBY = 112; // 15 minutes = 112 x 8s
const int MODE_NORMAL = 15; // 2 minutes = 15 x 8s

// Temperature des modes d'hibernation
const int TEMPERATURE_ECONOMIE = -8;
const int TEMPERATURE_STANDBY = -1;
const int TEMPERATURE_DELTA_HIBERNATION = 7;

// Niveaux de la batterie
const float BATTERIE_NIVEAU_CRITIQUE = 5.5;
const float BATTERIE_NIVEAU_BAS = 6;

// Setup
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

// Fonction pour lire la tension d'alimentation, en millivolts
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

// Lecture de la tension de la batterie, en volts
float getBatt() {
  float vcc = readVcc()/1000;
  // 512, à cause du diviseur de tension (x2)
  return (analogRead(battPin) * vcc / 512);
}

// Lecture de la temperature
float getTemp() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

// Lecture de la pression
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

// Selection du bon temps d'hibernation selon la situation
int getModeHibernation(float temperature, float batterie){
  if(batterie < BATTERIE_NIVEAU_CRITIQUE) {
    return MODE_HIBERNATION;
  } else if (batterie < BATTERIE_NIVEAU_BAS) {
    if(temperature < TEMPERATURE_STANDBY) {
      return MODE_ECONOMIE;
    } else {
      return MODE_STANDBY;
    }
  } else {
    if(temperature < TEMPERATURE_ECONOMIE) {
      return MODE_ECONOMIE;
    } else if (temperature < TEMPERATURE_STANDBY) {
      return MODE_STANDBY;
    } else {
      return MODE_NORMAL;
    }
  }
}

// Met le µC en hibernation le temps requis
// Verifie si l'hibernation doit être interrompue
void hibernation(float batt, float tempDepart) {
  
  int compteur = 0;

  for(int i=0; i<getModeHibernation(tempDepart, batt); ++i){
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    
    // Chaque 2 minutes,
    if(++compteur >= 15) {
      // Si le niveau de la batterie n'est pas critique,
      if(batt > BATTERIE_NIVEAU_CRITIQUE) {
        // On vérifie si la température a monté suffisamment
        // pour avoir besoin d'arrêter l'hibernation
        if((getTemp()-tempDepart) >= TEMPERATURE_DELTA_HIBERNATION) {
          return;
        }
      }
      compteur = 0;
    }
  }
}

// Boucle principale
void loop() {
  // Valeurs
  float tempC = getTemp();
  float batt = getBatt();

  // Construit le paquet
  LoRa.beginPacket();
  LoRa.print(zone);
  LoRa.print(";");

  //Lecture de la pression uniquement si proche de 0°C
  if(tempC > TEMPERATURE_STANDBY) {
    LoRa.print(getPress());
  }

  LoRa.print(";");
  LoRa.print(tempC);
  LoRa.print(";");
  LoRa.print(getBatt());
  LoRa.print(";");
  LoRa.endPacket();

  hibernation(batt, tempC);
}
