
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
