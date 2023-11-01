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
// #include <liquidCrystal.h>
#include "EmonLib.h"
EnergyMonitor SCT013;

//===============================================
// Dados do WiFi
char ssid[] = "Wemos";
char pass[] = "123456789";

BlynkTimer timer;
//===============================================

//Mapeamento de hardware
#define relay_Pin D2

// #define S0 D4
#define S2 D1
// #define S2 D6 //Pinos de seleção do multiplexador

// int pinSCT = A0;   //Pino analógico conectado ao SCT-013
int portaSensores = A0; //Pino de entrada do multiplexador
int pinSCT = A0;

//===============================================
//Definição de Variáveis

//Sensor de Corrente
int tensao = 220;
int potencia;
double Irms = 0;

//controle da corrente da bomba
int correnteMinima = 2;
int correnteMaxima = 3;

//Sensor de Tensão
float tensaoMedida = 0;

//controle da Tensão da Rede
int tensaoMinima = 200;

//Controle do Blynk
int selecaoValue = 0;

//Controle de falha da bomba
int falhaBomba = 0; 
unsigned long inicioTempoFalha = 0;
unsigned long intervalo = 10000; //10s
//===============================================
//Funções de controle do Blynk

BLYNK_WRITE(V6)
{
  //recebe o valor do blynk
  selecaoValue = param.asInt();
  // Atualiza o estado do V5
  if(selecaoValue == 0){
    Blynk.virtualWrite(V5, 0);
  }
}

BLYNK_WRITE(V5)
{
  //recebe o valor do blynk
  int value = param.asInt();
  // Atualiza o estado do V6
  if(value == 1){
    Blynk.virtualWrite(V6, 1); //Troca o acionamento da bomba para manual
  }
  //aciona o relé se a seleção estiver em manual (1)
  if(selecaoValue == 1){
    digitalWrite(relay_Pin, value);
  }

}

// This function is called every time the device is connected to the Blynk.Cloud
//função chamada sempre que o dispositivo estiver conectado ao Blynk.Cloud  
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
  pinMode(relay_Pin, OUTPUT);
  digitalWrite(relay_Pin, LOW);

  SCT013.current(pinSCT, 6.0606); //Reajusta a corrente máxima do sensor para 10A 
  // pinMode(Sensor_Tensao, INPUT);
  pinMode(portaSensores, INPUT);

  // Debug console
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

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

  tensaoMedida = (Valor_Tensao_Lido * 220) / 1023.0;
  Serial.println(analogRead(portaSensores));

  //========================================================
  //Tratamento dos dados para acionar a bomba

  if(selecaoValue == 0){
    if(Irms == 0 && falhaBomba == 0){
      digitalWrite(relay_Pin, HIGH);
    } else if(Irms < correnteMinima || Irms > correnteMaxima || tensaoMedida < tensaoMinima){
      digitalWrite(relay_Pin, LOW);
      falhaBomba = 1; //ocorreu a falha
      inicioTempoFalha = millis();
    }
  }

  if(falhaBomba == 1) {
    unsigned long tempoAtual = millis();
    unsigned long diferencaTempo = tempoAtual - inicioTempoFalha;

    if(diferencaTempo >= intervalo) {
      falhaBomba = 0;
    }
  }
  if(falhaBomba == 0){
    Serial.println("Bomba OK!");
  } else {
    Serial.println("Falha na bomba...");
  }

  //========================================================
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

}

 void ativarSensor(int sensor) {
  //Função para ativar a porta do multiplexador que será lida
  if(sensor == 1){
    Serial.println("Lendo o sensor de corrente.");
    //Ativa o pino 1 (2Y1 ou C1)
    analogWrite(S2, 255);
  }else if(sensor == 2){
    Serial.println("Lendo o sensor de tensão.");
    //Ativa o pino 2 (2Y0 ou C0)
    analogWrite(S2, 0);
  }
  delay(30); //intervalo de 30ms para troca da porta 
 }