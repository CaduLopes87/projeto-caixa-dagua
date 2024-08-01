// Inormações do Dispositivo fornecidas pelo Blynk
#define BLYNK_TEMPLATE_ID "TMPL2NAXG4qYq"
#define BLYNK_TEMPLATE_NAME "Projeto Poço"
#define BLYNK_AUTH_TOKEN "kCUgPuTvsh_1BHIlCsTYAjHvIP_zd_yk"

// Comente esta linha para desabilitar os prints e economizar espaço
#define BLYNK_PRINT Serial

//===============================================
//Inclusão de Bibliotecas

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "EmonLib.h"
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

#define Pin_Sensor_Tensao 34
#define pinSCT 35
#define led_conexao 2
#define led_bomba_falha 18
#define led_bomba_ok 19
#define relay_Pin 23
#define SDA_Pin 21
#define SCL_Pin 22
#define selecao_btn_Pin 4
#define controle_btn_Pin 5

ZMPT101B SensorTensao(Pin_Sensor_Tensao, 50.0);  // Inicia o sensor de tensão
EnergyMonitor SCT013;                            // Inicia o sensor de corrente

#define LCD_endr 0x27                                               // Cria endereço I2C do LCD e define tamanho
LiquidCrystal_I2C lcd(LCD_endr, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Inicia o LCD

//===============================================
//Definição de Variáveis

//Sensor de Corrente
int tensao = 220;
int potencia;
double Irms = 0;

//controle da corrente da bomba
float correnteMinima = 2.0;
float correnteMaxima = 3.5;

//Sensor de Tensão
float tensaoMedida = 0;

//controle da Tensão da Rede
int tensaoMinima = 200;

//Controle do Blynk
bool selecaoAutManValue = 1;  // 0 -> automática e 1 -> manual
bool releValue = 0;           // Recupera o valor do blynk para o Rele

//Controle da bomba e de falha da bomba
bool acionarBomba = false;           //Variável para controlar manualmente o relé
bool inicializacaoBomba = 0;         //desativada no início - verifica se foi iniciada
int tempoInicializacaoBomba = 2000;  // dois segundos para iniciar

bool falhaBomba = 0;
unsigned long inicioTempoFalha = 0;
unsigned long intervaloCorrecaoErro = 10000;  //10s

int tentativasInicializacao = 0;  // armazena a quantidade de tentativas de inicialização da bomba após o erro
bool sistemaCorrompido = false;   // diz se o sistema está corrompido

// Variáveis para o LCD
String LCDMensagem = "";
float LCDCorrente = 0;
int LCDTensao = 0;
String LCDselecaoAM = "";

//===============================================
//Funções de controle do Blynk

BLYNK_WRITE(V0) {
  correnteMinima = param.asFloat();
}

BLYNK_WRITE(V3) {
  correnteMaxima = param.asFloat();
}

BLYNK_WRITE(V7) {
  tensaoMinima = param.asInt();
}

BLYNK_WRITE(V6) {
  selecaoAutManValue = param.asInt();
  // Atualiza o estado do V5
  if (selecaoAutManValue == 0) {
    //Se automático
    Blynk.virtualWrite(V5, 0);
    acionarBomba = false;
    LCDselecaoAM = "A";
  } else {
    LCDselecaoAM = "M";
  }
}

BLYNK_WRITE(V5) {
  //recebe o valor do blynk
  releValue = param.asInt();
  // Atualiza o estado do V6
  if (releValue == 1) {
    Blynk.virtualWrite(V6, 1);  //Troca o acionamento da bomba para manual
    selecaoAutManValue = 1;
  }

  if (selecaoAutManValue == 1) {
    //aciona o relé de acordo com o botão se está em manual
    // digitalWrite(relay_Pin, ReleValue);
    acionarBomba = releValue;
  }
}

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

  pinMode(Pin_Sensor_Tensao, INPUT);
  SCT013.current(pinSCT, 1.5);  // Current: input pin, calibration.
  pinMode(led_conexao, OUTPUT);
  pinMode(led_bomba_ok, OUTPUT);
  pinMode(led_bomba_falha, OUTPUT);

  pinMode(selecao_btn_Pin, INPUT);
  pinMode(controle_btn_Pin, INPUT);

  SensorTensao.setSensitivity(1050);

  Wire.begin(SDA_Pin, SCL_Pin);  // Inicializa a comunicação I2C

  LCDMensagem = "Iniciando...";
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);
  lcd.setCursor(0, 0);
  lcd.print(LCDMensagem);

  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Setup a function to be called every second
  timer.setInterval(1000L, myTimerEvent);
  timer.setInterval(1000L, leituraSensores);
}

void loop() {
  Blynk.run();
  timer.run();
}

void leituraSensores() {
  lerBotoes();

  Serial.print("Corrente máxima: ");
  Serial.println(correnteMaxima);
  Serial.print("Corrente mínima: ");
  Serial.println(correnteMinima);
  Serial.print("Tensão mínima: ");
  Serial.println(tensaoMinima);
  //Impressão dos resultados no display LCD
  lcd.setBacklight(HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(LCDMensagem);
  lcd.setCursor(0, 1);
  lcd.print(LCDTensao);
  lcd.print("V");
  lcd.setCursor(6, 1);
  lcd.print(String(LCDCorrente, 1));  //Mostra a varíável com apenas uma casa decimal
  lcd.print("A");
  lcd.setCursor(12, 1);
  lcd.print(LCDselecaoAM);

  //Acende o led se conectado ao Blynk e pisca se não conectado
  if (Blynk.connected()) {
    digitalWrite(led_conexao, HIGH);
    LCDMensagem = "Conectado!";
  } else {
    piscarLed(led_conexao, 500);
    LCDMensagem = "";
    LCDMensagem = "Desconectado!";
  }

  //========================================================
  //Funcionamento do Sensor de Corrente
  Irms = SCT013.calcIrms(1480);  // Calcula o valor da Corrente 1172

  potencia = Irms * tensao;  // Calcula o valor da Potencia Instantanea

  LCDCorrente = Irms;

  //========================================================
  //Funcionamento do Sensor de Tensão
  tensaoMedida = SensorTensao.getRmsVoltage(20);

  LCDTensao = tensaoMedida;

  Serial.print("Tensão lida: ");
  Serial.println(tensaoMedida);

  //========================================================
  //Tratamento dos dados para acionar a bomba
  if (selecaoAutManValue == 1) {  //Se manual
    falhaBomba = 0;
    sistemaCorrompido = 0;
    inicializacaoBomba = 0;
    tentativasInicializacao = 0;
    digitalWrite(relay_Pin, acionarBomba);
    digitalWrite(led_bomba_ok, HIGH);
    digitalWrite(led_bomba_falha, HIGH);

  } else if ((selecaoAutManValue == 0) && (sistemaCorrompido == 0)) {  //Se automático e não corrompido
    if (inicializacaoBomba == 0 && falhaBomba == 0) {
      digitalWrite(relay_Pin, LOW);
      delay(tempoInicializacaoBomba);  //intervalo para ligar a bomba
      inicializacaoBomba = 1;          //armazena que a bomba foi inciada
    } else if ((inicializacaoBomba == 1) && (Irms <= correnteMinima || Irms >= correnteMaxima || tensaoMedida <= tensaoMinima)) {
      digitalWrite(relay_Pin, HIGH);
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

  if (falhaBomba == 0 && selecaoAutManValue == 0) {
    Serial.println("Bomba OK!");
    digitalWrite(led_bomba_ok, HIGH);
    digitalWrite(led_bomba_falha, LOW);
  } else if (falhaBomba == 1 && sistemaCorrompido != 1) {
    Serial.begin(115200);
    Serial.println("Falha na bomba...");
    digitalWrite(led_bomba_falha, HIGH);
    digitalWrite(led_bomba_ok, LOW);
  } else if (sistemaCorrompido == 1) {
    Serial.println("Sistema Corrmpido!!!!");
    digitalWrite(led_bomba_ok, LOW);
    piscarLed(led_bomba_falha, 500);
    LCDMensagem = "";
    LCDMensagem = "Sistema em Falha";
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
  delay((set_intervalo / 10));
}

void lerBotoes() {
  int botaoSelecao = digitalRead(selecao_btn_Pin);
  int botaoControle = digitalRead(controle_btn_Pin);

  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.print("BOTÃO SELEÇÃO: ");
  Serial.println(botaoSelecao);
  Serial.print("BOTÃO CONTROLE: ");
  Serial.println(botaoControle);

  if (botaoSelecao == 1) {
    selecaoAutManValue = inverterValorBooleano(selecaoAutManValue);

    Blynk.virtualWrite(V6, selecaoAutManValue);
    Serial.print("ESTADO SELEÇÃO: ");
    Serial.println(selecaoAutManValue);
    Serial.println();
    Serial.println();
    Serial.println();
  }

  if (botaoControle == 1) {
    acionarBomba = inverterValorBooleano(acionarBomba);

    Serial.print("ACIONAR BOMBA: ");
    Serial.println(acionarBomba);
    releValue = acionarBomba;
    Blynk.virtualWrite(V5, acionarBomba);
  }
}

void acionarReleManualmente() {
}

bool inverterValorBooleano(bool valor) {
  bool valorAInverter = valor;

  if (valorAInverter == true) {
    valorAInverter = false;
  } else {
    valorAInverter = true;
  }

  return valorAInverter;
}
