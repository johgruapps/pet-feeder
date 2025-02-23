#include <Adafruit_SoftServo.h>  // SoftwareServo (works on non PWM pins)
Adafruit_SoftServo servo;
#define servoPin 0   // Servo control line (orange) on Trinket Pin #0
#define openPosition 10
#define closedPosition 180

#define feedingSize 20

int led = 1; // blink 'digital' pin 1 - AKA the built in red LED

void setup() {
  // Set up the interrupt that will refresh the servo for us automagically
  OCR0A = 0xAF;            // any number is OK
  TIMSK |= _BV(OCIE0A);    // Turn on the compare interrupt (below!)

  // initialize the digital pin as an output.
  pinMode(led, OUTPUT);
  servo.attach(servoPin);   // Attach the servo to pin 0 on Trinket
  servo.write(closedPosition);
  flashLed();

  open();
  flashLed();

  close();
  flashLed();

  //Make sure the feeder is closed!
  servo.write(closedPosition);
}

void loop() {
  //Nothing here.
}

void flashLed() {
  //Flash the LED multiple times
  for (int flash = 0; flash <= 20; flash++) {
    digitalWrite(led, HIGH); 
    delay(40);
    digitalWrite(led, LOW);
    delay(60);
  }
}

void blinkLed() {
  //Blink the LED one time.
  digitalWrite(led, HIGH); 
  delay(50);
  digitalWrite(led, LOW);
  delay(50);
}

void jerky() {
  // "Shakes" the feeder to spit out more food
  for (int jerk = 0; jerk <= feedingSize; jerk++) {
    servo.write(40);
    delay(50);

    servo.write(0);
    delay(20); 

    blinkLed();
  }
  servo.write(openPosition);
  delay(1500); 
}

void open(){
  for (int blink = 0; blink <= 4; blink++) {
    blinkLed();
  }

  // Smooth servo movement
  for (int pos = closedPosition; pos >= openPosition; pos--) {
    servo.write(pos);
    delay(20); // Adjust for smoothness
  }
  jerky();
}

void close(){
  for (int blink = 0; blink <= 2; blink++) {
    blinkLed();
  }

  // Smooth servo movement
  for (int pos = openPosition; pos <= closedPosition; pos++) {
    servo.write(pos);
    delay(20); // Adjust for smoothness
  }
}

// We'll take advantage of the built in millis() timer that goes off
// to keep track of time, and refresh the servo every 20 milliseconds
volatile uint8_t counter = 0;
SIGNAL(TIMER0_COMPA_vect) {
  // this gets called every 2 milliseconds
  counter += 2;
  // every 20 milliseconds, refresh the servos!
  if (counter >= 20) {
    counter = 0;
    servo.refresh();
  }
}
