

#include <IRremote.hpp>

#define IR_DATA 0 //Input from the IR receiver
#define IR_RAW 1 // ALSO input from the IR receiver, pins 0 and 1 are bridged. This allows the Teensy to use one to know when the packet starts, without having to modify the IRremote lib.
#define SWITCH_OUTPUT 2 // Output to the BJT transistor that closes the mill's probe circuit
#define INTERNAL_LED 13 
#define INDICATOR_LED 3 // Battery indicator LED
#define PROBE_TRIGGER_PERIOD 5 // Milliseconds for the mill circuit to close after the start of the packet. The actual packet lasts about 25ms, which I have found to be too long for the mill probing routines to function correctly.
#define BLINK_PERIOD 500 // LED blink rate in milliseconds
#define MAX_BATT_VOLTS 3.6
#define MIN_BATT_VOLTAGE 1.5 // Probe volts before the LED starts a warning flash
unsigned long packetStartTime = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastIndicatorLEDUpdate = 0;
uint8_t battVoltageRaw = 0;
float battVoltage = 0;

enum ProbeState
{
  STARTUP,   // Waiting for initial probe signal
  SLEEPING,  // Open mill circuit
  TRIGGERED  // Closed mill circuit
};
ProbeState probeState;

void setup() 
{
  pinMode(IR_RAW, INPUT);
  pinMode(SWITCH_OUTPUT, OUTPUT);
  pinMode(INDICATOR_LED, OUTPUT);
  pinMode(INTERNAL_LED, OUTPUT);
  digitalWrite(INDICATOR_LED, HIGH);
  IrReceiver.begin(IR_DATA);
  probeState = STARTUP;
}

void loop() 
{
  // Handle IR signals
  if(!digitalRead(IR_RAW) && !packetStartTime) // If its the start of an IR packet
  {
    packetStartTime = millis(); // Record the start time
  }
  if (IrReceiver.decode()) // If the packet is complete
  {
    // Extract the battery data, convert to volts.
    battVoltageRaw = IrReceiver.decodedIRData.command;
    battVoltage = map((float)battVoltageRaw, 0, 127, 0, MAX_BATT_VOLTS);

    // Reset the packet start
    packetStartTime = 0;
    IrReceiver.resume();
  }

  // Evaluate probe state
  if(millis() > packetStartTime + PROBE_TRIGGER_PERIOD) // If we've been triggered for the PROBE_TRIGGER_PERIOD, go back to sleep
  {
    probeState = SLEEPING;
  }
  else // Otherwise, trigger
  {
    probeState = TRIGGERED;
  }

  // Evaluate battery voltage
  if(battVoltage == 0) // Off if no battery data is received
  {
    digitalWrite(INDICATOR_LED, LOW);
  }
  else if(battVoltage > MAX_BATT_VOLTS) // Solid if battery is good
  {
    digitalWrite(INDICATOR_LED, HIGH);
  }
  else // Blink otherwise (if battery is low)
  {
    if(millis() > lastIndicatorLEDUpdate + BLINK_PERIOD)
    {
      digitalWrite(INDICATOR_LED, !digitalRead(INDICATOR_LED));
      lastIndicatorLEDUpdate = millis();
    }
  }
  
  // Execute probe state
  if(probeState == STARTUP)
  {
    // Toggling the switch here will cause the Tormach to lock out until it gets a good signal from the probe. It does mean that the probe will need to be actuated manually as part of the startup, but it should help prevent crashes.
    if(millis() > lastLEDUpdate + BLINK_PERIOD)
    {
      digitalWrite(SWITCH_OUTPUT, !digitalRead(SWITCH_OUTPUT));
      digitalWrite(INTERNAL_LED, !digitalRead(INTERNAL_LED));
      lastLEDUpdate = millis();
    }
  }  
  else if(probeState == SLEEPING)
  {
    // Disable the mill switch
    digitalWrite(SWITCH_OUTPUT, LOW);
    digitalWrite(INTERNAL_LED, LOW);
  }  
  else if(probeState == TRIGGERED)
  {
    // Enable the mill switch
    digitalWrite(SWITCH_OUTPUT, HIGH);
    digitalWrite(INTERNAL_LED, HIGH);
  }
  

}
