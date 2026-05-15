/*Use delay() com cuidado, pois ele bloqueia a execução. Prefira usar millis() para controlar o tempo.
 * 
Para ler comandos da Serial, use: if (Serial.available() > 0) { String cmd = Serial.readStringUntil('\n'); }

Para extrair parâmetros de comandos, use cmd.substring(), cmd.indexOf() ou sscanf().

O ESP32 usa analogWrite() com valores de 0-255 para PWM no LED.

Teste cada comando individualmente no Monitor Serial antes de integrar com o driver.
*/



// Defina os pinos de LED e LDR
// Defina uma variável com valor máximo do LDR (4000)
// Defina uma variável para guardar o valor atual do LED (10)
int ledPin = 18;
int ledValue = 10;
int ldrValue = 0;
int ldrTreshold = 100;

int ldrPin = 15;
// Faça testes no sensor ldr para encontrar o valor maximo e atribua a variável ldrMax
int ldrMax = 4095;

unsigned long previousMillis = 0;
const int interval = 2000;

void setup() {
    Serial.begin(9600);
    
    pinMode(ledPin, OUTPUT);
    pinMode(ldrPin, INPUT);

    Serial.printf("SmartLamp Initialized.\n");

}

// Função loop será executada infinitamente pelo ESP32
void loop() {
  //Obtenha os comandos enviados pela serial 
  //e processe-os com a função processCommand
  unsigned long currentMillis = millis();
  
  if(currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    Serial.print("Valor atual do ldr: ");
    Serial.println(ldrGetValue());
  }
  
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    Serial.print("Voce digitou: ");
    Serial.println(command);
    
    processCommand(command);
  }
  
  // Automaticamente desliga se o valor de ldr for menor que o treshold.
  if(ldrValue <= ldrTreshold) {
    analogWrite(ledPin, LOW);  
  } else {
    analogWrite(ledPin, ledValue);
  }

}


// compare o comando com os comandos possíveis e execute a ação correspondente      
void processCommand(String commandRaw) {

  int spaceBarIndex= commandRaw.indexOf(" ");
  String parameter= commandRaw.substring(spaceBarIndex + 1);
  String command = commandRaw.substring(0, spaceBarIndex);
  
  // Testes 
  /*
  Serial.print("Indice do parametro: ");
  Serial.println(spaceBarIndex + 1);
  Serial.print("Parametro: ");
  Serial.println(parameter);
  Serial.print("Comando: ");
  Serial.println(command);
  */
  
  //SET_LED X - Define a intensidade do LED (X entre 0-100). Valores fora desse intervalo devem ser ignorados e retornar erro.
  if(command == "SET_LED") {
    ledUpdate(parameter.toInt());
  } else if(command == "GET_LED") {  //GET_LED - Retorna a intensidade atual do LED.
    Serial.print("Valor do led: ");
    Serial.println(round((ledValue/255.0) * 100.0));
  } else if(command == "GET_LDR") {  //GET_LDR - Retorna o valor da leitura atual do LDR (normalizado entre 0-100).
    Serial.print("Valor do ldr: ");
    Serial.println(ldrGetValue());
  } else if(command == "SET_TRESHOLD") { //SET_THRESHOLD X - Define o limiar de ativação automática do LED (X entre 0-100). O LED acende automaticamente quando a leitura normalizada do LDR ultrapassar esse valor.
    ldrTreshold = parameter.toInt();  
  } else if(command == "GET_TRESHOLD") { //GET_THRESHOLD - Retorna o valor atual do threshold.
    Serial.print("Valor treshold: ");
    Serial.println(ldrTreshold);
  } else {
    Serial.println("Comando não reconhecido. Comandos aceitos são: SET_LED <0-100>, GET_LED, SET_TRESHOLD <0-100>, GET_TRESHOLD, GET_LDR");
  }

}

// Função para atualizar o valor do LED
// Valor deve convertar o valor recebido pelo comando SET_LED para 0 e 255
// Normalize o valor do LED antes de enviar para a porta correspondente 
void ledUpdate(int value) {
  
    if(value > 100 || value < 0) {
      Serial.println("Valor invalido!");
      return;
    }
    
    float percent = float(value/100.0);

    ledValue = round(255*percent);
    analogWrite(ledPin, ledValue);
}

// Função para ler o valor do LDR
int ldrGetValue() {
    // Leia o sensor LDR e retorne o valor normalizado entre 0 e 100
    // faça testes para encontrar o valor maximo do ldr (exemplo: aponte a lanterna do celular para o sensor)       
    // Atribua o valor para a variável ldrMax e utilize esse valor para a normalização
    
    ldrValue = analogRead(ldrPin);
    ldrValue = round((ldrValue/float(ldrMax)) * 100.0);

    //Serial.print("Valor ldr (normalizado): ");
    //Serial.println(ldrValue);
    
    return ldrValue;

}
