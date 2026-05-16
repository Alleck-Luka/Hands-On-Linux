int ledPin = 18;
int ledValue = 10;
int ldrValue = 0;
int ldrTreshold = 100;

int ldrPin = 15;
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
  unsigned long currentMillis = millis();
  
  if(currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    Serial.print("RES GET_LDR ");
    Serial.println(ldrGetValue());
  }
  
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    processCommand(command);
  }
  
  // Automaticamente desliga se o valor de ldr for menor que o treshold.
  if(ldrValue <= ldrTreshold) {
    analogWrite(ledPin, LOW);  
  } else {
    analogWrite(ledPin, ledValue);
  }

}


void processCommand(String commandRaw) {

  int spaceBarIndex= commandRaw.indexOf(" ");
  String parameter= commandRaw.substring(spaceBarIndex + 1);
  String command = commandRaw.substring(0, spaceBarIndex);
  
  if(command == "SET_LED") {
    ledUpdate(parameter.toInt());
  } else if(command == "GET_LED") {
    Serial.print("RES GET_LED: ");
    Serial.println(round((ledValue/255.0) * 100.0));
  } else if(command == "GET_LDR") {
    Serial.print("RES GET_LDR: ");
    Serial.println(ldrGetValue());
  } else if(command == "SET_TRESHOLD") {
    int val = parameter.toInt();
    if(val < 0 || val > 100) return;
    ldrTreshold = val;
    Serial.println("RES SET_TRESHOLD 1");
  } else if(command == "GET_TRESHOLD") {
    Serial.print("RES GET_THRESHOLD ");
    Serial.println(ldrTreshold);
  } else {
    Serial.println("ERR Unknown command. - Comando inválido ou não reconhecido.");
  }

}

// Função para atualizar o valor do LED
// Valor deve convertar o valor recebido pelo comando SET_LED para 0 e 255
// Normalize o valor do LED antes de enviar para a porta correspondente 
void ledUpdate(int value) {
  
    if(value > 100 || value < 0) {
      Serial.println("RES SET_LED -1");
      return;
    }
    
    float percent = float(value/100.0);

    ledValue = round(255*percent);
    analogWrite(ledPin, ledValue);
    Serial.println("RES SET_LED 1");
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