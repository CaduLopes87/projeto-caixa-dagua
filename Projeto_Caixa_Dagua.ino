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
#include <ZMPT101B.h>

//===============================================
// Dados do WiFi
char ssid[] = "Wemos";
char pass[] = "123456789";
// char ssid[] = "CEUNET - Bezerra_2G";
// char pass[] = "A24b27*9";

BlynkTimer timer;
//===============================================

//Mapeamento de hardware

//Leds de  aviso
#define Pin_Sensor_Tensao A0
#define relay_Pin D2
#define led_bomba_ok D3
#define led_bomba_falha D4
#define led_conexao D5

int pinSCT = D6;  //Pino analógico conectado ao SCT-013

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
bool selecaoAutManValue = 0;  // 0 -> automática e 1 -> manual
bool estadoChaveSelecao = 1;

//Controle da bomba e de falha da bomba
bool inicializacaoBomba = 0;         //desativada no início
int tempoInicializacaoBomba = 2000;  // dois segundos para iniciar

bool falhaBomba = 0;
unsigned long inicioTempoFalha = 0;
unsigned long intervaloCorrecaoErro = 10000;  //10s

int tentativasInicializacao = 0;  // armazena a quantidade de tentativas de inicialização da bomba após o erro
bool sistemaCorrompido = false;   // diz se o sistema está corrompido

//===============================================
//Funções de controle do Blynk

BLYNK_WRITE(V6) {
  //recebe o valor do blynk
  selecaoAutManValue = param.asInt();
  // Atualiza o estado do V5
  if (selecaoAutManValue == 0) {  //Se automático
    Blynk.virtualWrite(V5, 0);    //passível de retirar
  }
}

BLYNK_WRITE(V5) {
  //recebe o valor do blynk
  int ReleValue = param.asInt();
  // Atualiza o estado do V6
  if (ReleValue == 1) {
    Blynk.virtualWrite(V6, 1);  //Troca o acionamento da bomba para manual
    selecaoAutManValue = 1;
  }

  if (selecaoAutManValue == 1) {
    //aciona o relé de acordo com o botão se está em manual
    digitalWrite(relay_Pin, ReleValue);
  }
}

// This function is called every time the device is connected to the Blynk.Cloud
//função chamada sempre que o dispositivo estiver conectado ao Blynk.Cloud
BLYNK_CONNECTED() {
  // Change Web Link Button message to "Congratulations!"
  Blynk.setProperty(V3, "offImageUrl", "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations.png");
  Blynk.setProperty(V3, "onImageUrl", "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations_pressed.png");
  Blynk.setProperty(V3, "url", "https://docs.blynk.io/en/getting-started/what-do-i-need-to-blynk/how-quickstart-device-was-made");
}

// This function sends Arduino's uptime every second to Virtual Pin 2.
void myTimerEvent() {
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(V4, millis() / 800);
}

void setup() {
  //Definindo portas
  pinMode(relay_Pin, OUTPUT);
  digitalWrite(relay_Pin, LOW);

  SCT013.current(pinSCT, 6.0606);  //Reajusta a corrente máxima do sensor para 10A
  pinMode(Pin_Sensor_Tensao, INPUT);

  pinMode(led_conexao, OUTPUT);
  pinMode(led_bomba_ok, OUTPUT);
  pinMode(led_bomba_falha, OUTPUT);

  // Debug console
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Setup a function to be called every second
  timer.setInterval(1000L, myTimerEvent);
}

void loop() {
  Blynk.run();
  timer.run();

  //Acende o led se conectado ao Blynk e pisca se não conectado
  if (Blynk.connected()) {
    digitalWrite(led_conexao, HIGH);
  } else {
    piscarLed(led_conexao, 500);
  }

  //========================================================
  //Funcionamento do Sensor de Corrente

  Irms = SCT013.calcIrms(1480);  // Calcula o valor da Corrente 1172

  potencia = Irms * tensao;  // Calcula o valor da Potencia Instantanea

  //========================================================
  //Funcionamento do Sensor de Tensão

  int Valor_Tensao_Lido = analogRead(Pin_Sensor_Tensao);
  Serial.print("Tensão bruta: ");
  Serial.println(Valor_Tensao_Lido);

  //124 = 220V
  //134 --> 220   tm = (VL * 220)/124
  //VL   --> tm

  tensaoMedida = (Valor_Tensao_Lido * 220) / 124;
  Serial.print("Tensão Decimal: ");
  Serial.println(tensaoMedida);
  int tensaoRealMedida = map(tensaoMedida, 0, 1023, 0, 220);
  Serial.print("Tensão Real: ");
  Serial.println(tensaoRealMedida);

  //========================================================
  //Tratamento dos dados para acionar a bomba
  if (selecaoAutManValue == 1) {  //Se manual
    falhaBomba = 0;
    sistemaCorrompido = 0;
    inicializacaoBomba = 0;
    tentativasInicializacao = 0;
    digitalWrite(led_bomba_ok, HIGH);
    digitalWrite(led_bomba_falha, HIGH);

  } else if ((selecaoAutManValue == 0) && (sistemaCorrompido == 0)) {  //Se automático
    if (inicializacaoBomba == 0 && falhaBomba == 0) {
      digitalWrite(relay_Pin, HIGH);
      delay(tempoInicializacaoBomba);  //intervalo para ligar a bomba
      inicializacaoBomba = 1;          //armazena que a bomba foi inciada
    } else if ((inicializacaoBomba == 1) && (Irms <= correnteMinima || Irms >= correnteMaxima || tensaoMedida <= tensaoMinima)) {
      digitalWrite(relay_Pin, LOW);
      falhaBomba = 1;  //ocorreu a falha
      inicioTempoFalha = millis();
      inicializacaoBomba = 0;
    }

    if ((falhaBomba == 1) && (sistemaCorrompido == 0)) {
      unsigned long tempoAtual = millis();
      unsigned long diferencaTempo = tempoAtual - inicioTempoFalha;

      if (diferencaTempo >= intervaloCorrecaoErro) {
        falhaBomba = 0;
        tentativasInicializacao++;
      }
    }

    if (tentativasInicializacao >= 3) {
      sistemaCorrompido = 1;  //Não foi possível ligar a bomba
      falhaBomba = 1;

      Serial.println("Sistema Corrompido!");
    }
  }

  if (falhaBomba == 0) {
    Serial.println("Bomba OK!");
    digitalWrite(led_bomba_ok, HIGH);
    digitalWrite(led_bomba_falha, LOW);
  } else if (falhaBomba == 1 && sistemaCorrompido != 1) {
    Serial.println("Falha na bomba...");
    digitalWrite(led_bomba_falha, HIGH);
    digitalWrite(led_bomba_ok, LOW);
  } else if (sistemaCorrompido == 1) {
    Serial.println("Sistema Corrmpido!!!!");
    digitalWrite(led_bomba_ok, LOW);
    piscarLed(led_bomba_falha, 500);
  }

  //========================================================
  //Impressão dos resultados no monitor Serial
  Serial.print("Corrente = ");
  Serial.print(Irms);
  Serial.println(" A");
  Blynk.virtualWrite(V1, Irms);  //Envia a corrente para o aplicativo blynk
  Serial.print("Potencia = ");
  Serial.print(potencia);
  Serial.println(" W");

  Serial.print("Tensão = ");
  Serial.print(tensaoMedida);
  Serial.println(" V");
  Blynk.virtualWrite(V2, tensaoMedida);  //Envia a tensão para o aplicativo blynk
}

void piscarLed(int set_led, int set_intervalo) {
  digitalWrite(set_led, HIGH);
  delay(set_intervalo);
  digitalWrite(set_led, LOW);
  delay((set_intervalo/10));
}