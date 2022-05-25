#include <math.h>
#include <Venerate.h>
#include <SerialCommands.h>

const String DEVICE_TYPE = "et312";
const int FM_VERSION = 10000; // 1.00.00

Venerate EBOX = Venerate(0);

unsigned long starttime;
int modepick[] = { ETMODE_waves, ETMODE_stroke, ETMODE_rhythm, ETMODE_climb, ETMODE_orgasm, ETMODE_phase2, ETMODE_random2, ETMODE_phase2, ETMODE_random2, ETMODE_stroke, ETMODE_phase2 };
int nummodes = sizeof(modepick);

char serial_command_buffer[32];
SerialCommands serialCommands(&Serial, serial_command_buffer, sizeof(serial_command_buffer), "\n", " ");

void setup() {
    Serial.begin(9600);

    // Setup ET-312 stuff
    Serial1.begin(19200);
    EBOX.begin(Serial1);
    EBOX.setdebug(Serial, 0);
    starttime = millis();

    // Add commands
    serialCommands.SetDefaultHandler(commandUnrecognized);
    serialCommands.AddCommand(new SerialCommand("introduce", commandIntroduce));
    serialCommands.AddCommand(new SerialCommand("device-status", commandDeviceStatus));
    serialCommands.AddCommand(new SerialCommand("mode-set", commandModeSet));
    serialCommands.AddCommand(new SerialCommand("level-set", commandLevelSet));
    serialCommands.AddCommand(new SerialCommand("level-get", commandLevelGet));
    serialCommands.AddCommand(new SerialCommand("adc-enable", commandAdcEnable));
    serialCommands.AddCommand(new SerialCommand("adc-disable", commandAdcDisable));

    serialCommands.GetSerial()->write(0x07);
}

void loop() {
    if (!EBOX.isconnected()) {
        // Try to connect every couple of seconds, don't connect until first second
        if ((millis() - starttime) > 1500) {
           starttime = millis();
           //digitalWrite(13,HIGH); // maybe use -> https://hackaday.io/project/9017-skywalker-lightsaber/log/72354-adafruit-trinket-m0-controlling-the-onboard-dotstar-led-via-arduino-ide
           EBOX.hello();
           //digitalWrite(13,LOW);
        }
        delay(100);
    }
  
    serialCommands.ReadSerial();
}

/** 
 * ET-312 functions 
 */
void et312_set_mode(int p) {
    EBOX.setbyte(ETMEM_mode, p-1);
    EBOX.setbyte(ETMEM_command1, ETBUTTON_setmode);
    delay(180);
    EBOX.setbyte(ETMEM_command1, ETBUTTON_lockmode);
    delay(180);
}

// Enable ADC: You get control over the potentiometers.
void et312_enable_adc() {
    //  Read R15 from device
    int register15 = EBOX.getbyte(ETMEM_panellock);

    if (!(register15 & (1 << 0))) {
        return;
    }

    // Set bit 0 to False
    register15 = (register15 & ~(1 << 0));

    EBOX.setbyte(ETMEM_panellock, register15);
}

// Disable ADC: The potentiometers at the front are disabled so you can not control the device anymore!
void et312_disable_adc() {
    //  Read R15 from device
    int register15 = EBOX.getbyte(ETMEM_panellock);

    if ((register15 & (1 << 0))) {
        return;
    }

    // Set bit 0 to True
    register15 = (register15 | (1 << 0));

    EBOX.setbyte(ETMEM_panellock, register15);
}

// Read the ADC: Returns True -> ADC enabled / False -> ADC disabled.
boolean et312_adc_enabled() {
    //  Read R15 from device
    int register15 = EBOX.getbyte(ETMEM_panellock);

    return !(register15 & (1 << 0));
}

// Sets the level of channel: Adc needs to be enabled for this (see function et312_enable_adc)
boolean et312_set_level(char channel, int level) {
    if (et312_adc_enabled()) {
        // Level cannot be changed with enabled ADC
        return false;
    }

    if (level < 0x00 || level > 0xff) {
        // Level needs to be between 0x00 and 0xff
        return false;
    }

    int level_address;

    if ('A' == channel) {
        level_address = ETMEM_knoba;
    } else if ('B' == channel) {
        level_address = ETMEM_knobb;
    } else {
        return false;
    }

    return EBOX.setbyte(level_address, level);
}

int et312_get_level(char channel) {
    int level_address;

    if ('A' == channel) {
        level_address = ETMEM_knoba;
    } else if ('B' == channel) {
        level_address = ETMEM_knobb;
    } else {
        return -1;
    }

    return EBOX.getbyte(level_address);
}

/**
 * Commands
 */
void commandIntroduce(SerialCommands* sender) {
    sender->GetSerial()->println(DEVICE_TYPE + "," + FM_VERSION);
}

void commandUnrecognized(SerialCommands* sender, const char* cmd)
{
    sender->GetSerial()->println(strprintf("Unrecognized command [%s]", cmd));
}

void commandDeviceStatus(SerialCommands* sender)
{  
    String connected = "not_connected";
    
    if (EBOX.isconnected()) {
        connected = "connected";
    }

    sender->GetSerial()->println("device-status," + connected);
}

void commandModeSet(SerialCommands* sender)
{  
    char* modeStr = sender->Next();

    if (modeStr == NULL) {
        sender->GetSerial()->println("Mode parameter missing");
        return;
    }
  
    int mode = atoi(modeStr);

    if (!EBOX.isconnected()) {
        sender->GetSerial()->println(strprintf("mode-set,%d,failed:no_device_connected", mode));
        return;
    }

    if (mode < 1 || mode > nummodes) {
        sender->GetSerial()->println(strprintf("mode-set,%d,failed:mode_out_of_range", mode));
        return;
    }

    et312_set_mode(modepick[mode-1]);

    sender->GetSerial()->println(strprintf("mode-set,%d,success", mode));
}

void commandAdcEnable(SerialCommands* sender)
{  
    if (!EBOX.isconnected()) {
        sender->GetSerial()->println(strprintf("adc-enable,failed:no_device_connected"));
        return;
    }

    et312_enable_adc();

    sender->GetSerial()->println(strprintf("adc-enable,success"));
}

void commandAdcDisable(SerialCommands* sender)
{  
    if (!EBOX.isconnected()) {
        sender->GetSerial()->println(strprintf("adc-disable,failed:no_device_connected"));
        return;
    }

    et312_disable_adc();

    sender->GetSerial()->println(strprintf("adc-disable,success"));
}

void commandLevelSet(SerialCommands* sender)
{  
    char* channelStr = sender->Next();
    char* levelStr = sender->Next();

    if (channelStr == NULL) {
        sender->GetSerial()->println("Channel parameter missing");
        return;
    }

    if (levelStr == NULL) {
        sender->GetSerial()->println("Level parameter missing");
        return;
    }
  
    char channel = channelStr[0];
    int level = atoi(levelStr);

    if (!EBOX.isconnected()) {
        sender->GetSerial()->println(strprintf("level-set,%c,%d,failed:no_device_connected", channel, level));
        return;
    }

    if (level < 0 || level > 99) {
        sender->GetSerial()->println(strprintf("level-set,%c,%d,failed:level_out_of_range", channel, level));
        return;
    }

    int level_byte_value = (int) round(level * 255 * 0.01);

    et312_set_level(channel, level_byte_value);

    sender->GetSerial()->println(strprintf("level-set,%c,%d,success", channel, level));
}


void commandLevelGet(SerialCommands* sender)
{  
    char* channelStr = sender->Next();

    if (channelStr == NULL) {
        sender->GetSerial()->println("Channel parameter missing");
        return;
    }
  
    char channel = channelStr[0];

    if (!EBOX.isconnected()) {
        sender->GetSerial()->println(strprintf("level-get,%c,failed:no_device_connected", channel));
        return;
    }

    int level_byte_value = et312_get_level(channel);
    int level = (int) round(level_byte_value * 100 / 255);

    sender->GetSerial()->println(strprintf("level-get,%c,success:%d", channel, level));
}
