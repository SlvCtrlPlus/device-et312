#include <math.h>
#include <Venerate.h>
#include <SerialCommands.h>

const char* DEVICE_TYPE = "et312";
const int FM_VERSION = 10000; // 1.00.00
const int PROTOCOL_VERSION = 10000; // 1.00.00

const int LEVEL_BYTE_MAX = 255;
const int LEVEL_PERCENTAGE_MAX = 99;

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
    serialCommands.AddCommand(new SerialCommand("attributes", commandAttributes));
    serialCommands.AddCommand(new SerialCommand("status", commandStatus));
    serialCommands.AddCommand(new SerialCommand("get-connected", commandSetMode));
    serialCommands.AddCommand(new SerialCommand("set-mode", commandSetMode));
    serialCommands.AddCommand(new SerialCommand("get-mode", commandGetMode));
    serialCommands.AddCommand(new SerialCommand("set-levelA", commandSetLevelA));
    serialCommands.AddCommand(new SerialCommand("set-levelB", commandSetLevelB));
    serialCommands.AddCommand(new SerialCommand("get-levelA", commandGetLevelA));
    serialCommands.AddCommand(new SerialCommand("get-levelB", commandGetLevelB));
    serialCommands.AddCommand(new SerialCommand("set-adc", commandSetAdc));
    serialCommands.AddCommand(new SerialCommand("get-adc", commandGetAdc));

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


int level_to_percentage(int level) {
    return map(level, 0, LEVEL_BYTE_MAX, 0, LEVEL_PERCENTAGE_MAX);
}

int percentage_to_level(int percentage) {
    return map(percentage, 0, LEVEL_PERCENTAGE_MAX, 0, LEVEL_BYTE_MAX);
}

/**
 * Commands
 */
void commandIntroduce(SerialCommands* sender) {
    serial_printf(sender->GetSerial(), "introduce;%s,%d,%d\n", DEVICE_TYPE, FM_VERSION, PROTOCOL_VERSION);
}

void commandAttributes(SerialCommands* sender) {
    serial_printf(sender->GetSerial(), "attributes;connected:ro[bool],adc:rw[bool],mode:rw[118-140],levelA:rw[0-99],levelB:rw[0-99]\n");
}

void commandStatus(SerialCommands* sender)
{  
    if (!EBOX.isconnected()) {
        serial_printf(
            sender->GetSerial(), 
            "status;connected:%d,adc:,mode:,levelA:,levelB:\n",
            EBOX.isconnected()
        );
    } else {
        serial_printf(
            sender->GetSerial(), 
            "status;connected:%d,adc:%d,mode:%d,levelA:%d,levelB:%d\n",
            EBOX.isconnected(),
            et312_adc_enabled(), 
            et312_get_mode(), 
            level_to_percentage(et312_get_level('A')),
            level_to_percentage(et312_get_level('B'))
        );
    }
}

void commandGetConnected(SerialCommands* sender)
{
    serial_printf(sender->GetSerial(), "get-connected;%d;status:successful\n", EBOX.isconnected());
}

void commandSetMode(SerialCommands* sender)
{  
    char* modeStr = sender->Next();

    if (modeStr == NULL) {
        serial_printf(sender->GetSerial(), "set-mode;;status:failed,reason:mode_param_missing\n");
        return;
    }
  
    int mode = atoi(modeStr);

    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "set-mode;%d;status:failed,reason:no_device_connected\n", mode);
        return;
    }

    if (mode < 0x76 || mode > 0x8C) {
        serial_printf(sender->GetSerial(), "set-mode;%d;status:failed,reason:mode_out_of_range\n", mode);
        return;
    }

    et312_set_mode(mode);

    serial_printf(sender->GetSerial(), "set-mode;%d;successful\n", mode);
}

void commandGetMode(SerialCommands* sender)
{
    serial_printf(sender->GetSerial(), "get-mode;%d;status:successful\n", et312_get_mode());
}

void commandSetAdc(SerialCommands* sender)
{
    char* adcStr = sender->Next();

    if (adcStr == NULL) {
        serial_printf(sender->GetSerial(), "set-adc;;status:failed,reason:adc_param_missing\n");
        return;
    }
  
    int adc = atoi(adcStr);
  
    if (adc > 1 || adc < 0) {
        serial_printf(sender->GetSerial(), "set-adc;%s;status:failed,reason:adc_out_of_range\n", adcStr);
        return;
    }

    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "set-adc;%s;status:failed,reason:no_device_connected\n", adcStr);
        return;
    }

    if (adc == 1) {
        et312_enable_adc();
    } else {
        et312_disable_adc();
    }

    serial_printf(sender->GetSerial(), "set-adc;%s;status:successful\n", adcStr);
}

void commandGetAdc(SerialCommands* sender)
{
    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "get-adc;;status:failed,reason:no_device_connected\n", adcStr);
        return;
    }

    serial_printf(sender->GetSerial(), "get-adc;%d;status:successful\n", et312_adc_enabled());
}

void commandSetLevel(SerialCommands* sender, char channel)
{
    char* levelStr = sender->Next();

    if (levelStr == NULL) {
        serial_printf(sender->GetSerial(), "set-level%c;;status:failed,reason:level_param_missing\n");
        return;
    }
  
    int level = atoi(levelStr);

    if (level < 0 || level > LEVEL_PERCENTAGE_MAX) {
        serial_printf(sender->GetSerial(), "set-level%c;%d;status:failed,reason:level_out_of_range\n", channel, level);
        return;
    }

    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "set-level%c;%d;status:failed,reason:no_device_connected\n", channel, level);
        return;
    }

    int level_byte_value = percentage_to_level(level);

    et312_set_level(channel, level_byte_value);

    serial_printf(sender->GetSerial(), "set-level%c;%d;status:successful\n", channel, level);
}

void commandSetLevelA(SerialCommands* sender)
{  
    commandSetLevel(sender, 'A');
}

void commandSetLevelB(SerialCommands* sender)
{  
    commandSetLevel(sender, 'B');
}

void commandGetLevel(SerialCommands* sender, char channel)
{  
    if (!EBOX.isconnected()) {
        serial_printf(sender->GetSerial(), "get-level%c;;status:failed,reason:no_device_connected\n", channel);
        return;
    }

    int level_byte_value = et312_get_level(channel);
    int level = level_to_percentage(level_byte_value);

    serial_printf(sender->GetSerial(), "get-level%c;%d;status:successful\n", channel, level);
}

void commandGetLevelA(SerialCommands* sender)
{
    commandGetLevel(sender, 'A');
}

void commandGetLevelB(SerialCommands* sender)
{
    commandGetLevel(sender, 'B');
}
