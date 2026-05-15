/***
 * LED Blinking
 * Code Written by TechMartian
 */
const int ledPin = 18;
const int ldrPin = 15;
const int loops = 2;
int i = 0;
int valorLDR = 0;

void setup() {
  Serial.begin(9600);
  // setup pin 5 as a digital output pin
  // pinMode (ledPin, OUTPUT);
  pinMode (ldrPin, INPUT);
  delay(2000);
}
void loop() {
  /*while(i<loops) {
    digitalWrite (ledPin, HIGH);  // turn on the LED
    delay(2000); // wait for half a second or 500 milliseconds
    digitalWrite (ledPin, LOW); // turn off the LED
    delay(2000); // wait for half a second or 500 milliseconds  
    i++;
    Serial.println(i);
  }
      digitalWrite (ledPin, LOW); // turn off the LED
  */
  valorLDR = analogRead(ldrPin);
  Serial.println(valorLDR);
  delay(1000);
}