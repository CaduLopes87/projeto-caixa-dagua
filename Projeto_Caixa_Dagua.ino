/* Inormações do Device fornecidas pelo Blynk */
#define BLYNK_TEMPLATE_ID "TMPL2NAXG4qYq"
#define BLYNK_TEMPLATE_NAME "Projeto Poço"
#define BLYNK_AUTH_TOKEN "kCUgPuTvsh_1BHIlCsTYAjHvIP_zd_yk"

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

//===============================================
//Inclusão de Bibliotecas

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <liquidCrystal.h>
#include "Ultrasonic.h"
#include "EmonLib.h"
EnergyMonitor SCT013;

//===============================================
// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "CEUNET - Bezerra_2G";
char pass[] = "A24b27*9";

BlynkTimer timer;
//===============================================

//Mapeamento de hardware
#define led1 D5

#define S1 D4
#define S2 D5
#define S3 D6 //Pinos de seleção do multiplexador

// int pinSCT = A0;   //Pino analógico conectado ao SCT-013
int portaSensores = A0; //Pino de entrada do multiplexador
int pinSCT = A0;

// #define Sensor_Tensao A0 //Pino analógico conectado ao Sensor de Tensão

const int echoPin = D3; //PINO DIGITAL UTILIZADO PELO HC-SR04 ECHO(RECEBE)
const int trigPin = D4; //PINO DIGITAL UTILIZADO PELO HC-SR04 TRIG(ENVIA)
Ultrasonic ultrasonic(trigPin,echoPin); //INICIALIZANDO OS PINOS DO ARDUINO

//===============================================
//Definição de Variáveis

//Sensor de Corrente
int tensao = 220;
int potencia;
double Irms = 0;

//Sensor de Tensão
float tensaoMedida = 0;

//Sensor Ultrassom
int distancia; 
String result; 

//===============================================
//Funções de controle do Blynk
BLYNK_WRITE(V5)
{
  //get value from Blynk
  int value = param.asInt();
  // Update state
  digitalWrite(led1, value);
}


// This function is called every time the device is connected to the Blynk.Cloud
BLYNK_CONNECTED()
{
  // Change Web Link Button message to "Congratulations!"
  Blynk.setProperty(V3, "offImageUrl", "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations.png");
  Blynk.setProperty(V3, "onImageUrl",  "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations_pressed.png");
  Blynk.setProperty(V3, "url", "https://docs.blynk.io/en/getting-started/what-do-i-need-to-blynk/how-quickstart-device-was-made");
}

// This function sends Arduino's uptime every second to Virtual Pin 2.
void myTimerEvent()
{
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(V4, millis() / 1000);
}

void setup()
{
  //Definindo portas
  pinMode(led1, OUTPUT);
  SCT013.current(pinSCT, 6.0606); //Reajusta a corrente máxima do sensor para 10A 
  // pinMode(Sensor_Tensao, INPUT);
  pinMode(portaSensores, INPUT);

  pinMode(echoPin, INPUT); //DEFINE O PINO COMO ENTRADA (RECEBE)
  pinMode(trigPin, OUTPUT); //DEFINE O PINO COMO SAIDA (ENVIA)

  // Debug console
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // You can also specify server:
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, "blynk.cloud", 80);
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, IPAddress(192,168,1,100), 8080);

  // Setup a function to be called every second
  timer.setInterval(1000L, myTimerEvent);
}

void loop()
{
  Blynk.run();
  timer.run();

  //========================================================
  //Funcionamento do Sensor de Corrente

  ativarSensor(1);
  Irms = SCT013.calcIrms(1480);  // Calcula o valor da Corrente 1172

  potencia = Irms * tensao;  // Calcula o valor da Potencia Instantanea

  //========================================================
  //Funcionamento do Sensor de Tensão
  ativarSensor(2);

  int Valor_Tensao_Lido = analogRead(portaSensores);
  if(Valor_Tensao_Lido != 0){
    Valor_Tensao_Lido -= 110; //calibração do sensor de tensão
  }
  tensaoMedida = (Valor_Tensao_Lido * 25) / 1023.0;
  Serial.println(analogRead(portaSensores));

  //Tratamento dos dados para acionar a bomba

  if(Irms < 2 && tensaoMedida > 4) {
    digitalWrite(led1, HIGH);
  }

  //Impressão dos resultados no monitor Serial
  Serial.print("Corrente = ");
  Serial.print(Irms);
  Serial.println(" A");
  Blynk.virtualWrite(V1, Irms); //Envia a corrente para o aplicativo blynk
  Serial.print("Potencia = ");
  Serial.print(potencia);
  Serial.println(" W");

  Serial.print("Tensão = ");
  Serial.print(tensaoMedida);
  Serial.println(" V");
  Blynk.virtualWrite(V2, tensaoMedida); //Envia a tensão para o aplicativo blynk

  //========================================================
  //Código para funcionamento do Ultrassom
  hcsr04();
  Serial.print("Distancia: ");
  Serial.print(result);
  Serial.println("cm");
  Blynk.virtualWrite(V0, result); //Envia a distância para o aplicativo blynk
}

//Função para calcular a distância com ultrassom 
void hcsr04(){
    digitalWrite(trigPin, LOW); 
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    //FUNÇÃO RANGING, FAZ A CONVERSÃO DO TEMPO DE
    //RESPOSTA DO ECHO EM CENTIMETROS, E ARMAZENA
    //NA VARIAVEL "distancia"
    distancia = (ultrasonic.Ranging(CM)); //VARIÁVEL GLOBAL RECEBE O VALOR DA DISTÂNCIA MEDIDA
    result = String(distancia); //Converte o inteiro distância para string
    delay(500);
 }

 void ativarSensor(int sensor) {
  //Função para ativar a porta do multiplexador que será lida
  if(sensor == 1){
    Serial.println("Lendo o sensor de corrente.");
    //Ativa o pino 1 (2Y1)
    digitalWrite(S1, LOW);
    digitalWrite(S2, HIGH);
    digitalWrite(S3, LOW);
  }else if(sensor == 2){
    Serial.println("Lendo o sensor de tensão.");
    //Ativa o pino 2 (2Y0) 
    digitalWrite(S1, LOW);
    digitalWrite(S2, LOW);
    digitalWrite(S3, LOW);
  }
  delay(30); //intervalo de 30ms para troca da porta 
 }


