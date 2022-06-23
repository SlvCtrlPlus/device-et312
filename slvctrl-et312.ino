#include <math.h>
#include <Venerate.h>
#include <SerialCommands.h>

const char* DEVICE_TYPE = "et312";
const int FM_VERSION = 10000; // 1.00.00

Venerate EBOX = Venerate(0);

unsigned long starttime;

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
    serialCommands.AddCommand(new SerialCommand("status", commandStatus));
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

int et312_get_mode() {
    return EBOX.getbyte(ETMEM_mode);
}

void et312_set_mode(int p) {
    EBOX.setbyte(ETMEM_mode, p);
    EBOX.setbyte(ETMEM_command1, ETBUTTON_setmode);
    delay(180);
    EBOX.setbyte(ETMEM_command1, ETBUTTON_newmode);
    delay(180);
}

/**
 * Commands
 */
void commandIntroduce(SerialCommands* sender) {
    serial_printf(sender->GetSerial(), "%s,%d\n", DEVICE_TYPE, FM_VERSION);
}

void commandUnrecognized(SerialCommands* sender, const char* cmd)
{
    serial_printf(sender->GetSerial(), "Unrecognized command [%s]\n", cmd);
}

void commandStatus(SerialCommands* sender)
{  
    if (!EBOX.isconnected()) {
        serial_printf(
            sender->GetSerial(), 
            "status,connected:%d\n",
            EBOX.isconnected()
        );
    } else {
        serial_printf(
            sender->GetSerial(), 
            "status,connected:%d,adc:%d,mode:%d,levelA:%d,levelB:%d\n",
            EBOX.isconnected(),
            et312_adc_enabled(), 
            et312_get_mode(), 
            level_to_percentage(et312_get_level('A')),
            level_to_percentage(et312_get_level('B'))
        );
    }
}

void commandModeSet(SerialCommands* sender)
{  
    char* modeStr = sender->Next();

    if (modeStr == NULL) {
        serial_printf(sender->GetSerial(), "Mode parameter missing\n");
        return;
    }
  
    int mode = atoi(modeStr);

    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "mode-set,%d,failed:no_device_connected\n", mode);
        return;
    }

    if (mode < 0x76 || mode > 0x8C) {
        serial_printf(sender->GetSerial(), "mode-set,%d,failed:mode_out_of_range\n", mode);
        return;
    }

    et312_set_mode(mode);

    serial_printf(sender->GetSerial(), "mode-set,%d,success\n", mode);
}

void commandAdcEnable(SerialCommands* sender)
{  
    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "adc-enable,failed:no_device_connected\n");
        return;
    }

    et312_enable_adc();

    serial_printf(sender->GetSerial(), "adc-enable,success\n");
}

void commandAdcDisable(SerialCommands* sender)
{  
    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "adc-disable,failed:no_device_connected\n");
        return;
    }

    et312_disable_adc();

    serial_printf(sender->GetSerial(), "adc-disable,success\n");
}

void commandLevelSet(SerialCommands* sender)
{  
    char* channelStr = sender->Next();
    char* levelStr = sender->Next();

    if (channelStr == NULL) {
        serial_printf(sender->GetSerial(), "Channel parameter missing\n");
        return;
    }

    if (levelStr == NULL) {
        serial_printf(sender->GetSerial(), "Level parameter missing\n");
        return;
    }
  
    char channel = channelStr[0];
    int level = atoi(levelStr);

    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "level-set,%c,%d,failed:no_device_connected\n", channel, level);
        return;
    }

    if (level < 0 || level > 99) {
        serial_printf(sender->GetSerial(), "level-set,%c,%d,failed:level_out_of_range\n", channel, level);
        return;
    }

    int level_byte_value = map(level, 0, 99, 0, 255);

    et312_set_level(channel, level_byte_value);

    serial_printf(sender->GetSerial(), "level-set,%c,%d,success\n", channel, level);
}


void commandLevelGet(SerialCommands* sender)
{  
    char* channelStr = sender->Next();

    if (channelStr == NULL) {
        serial_printf(sender->GetSerial(), "Channel parameter missing\n");
        return;
    }
  
    char channel = channelStr[0];

    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "level-get,%c,failed:no_device_connected\n", channel);
        return;
    }

    int level_byte_value = et312_get_level(channel);
    int level = level_to_percentage(level_byte_value);

    serial_printf(sender->GetSerial(), "level-get,%c,success:%d\n", channel, level);
}

int level_to_percentage(int level) {
    return map(level, 0, 255, 0, 99);
}
