void serial_printf(Stream *serial, const char* format, ...) {
  va_list args;
  va_start(args, format);

  int bufferSize = vsnprintf(NULL, 0, format, args);
  bufferSize++; // safe byte for \0

  char buffer[bufferSize];

  vsnprintf(buffer, bufferSize, format, args);

  va_end(args);

  serial->print(buffer);
}

void commandUnrecognized(SerialCommands* sender, const char* cmd)
{
    serial_printf(sender->GetSerial(), "Unrecognized command [%s]\n", cmd);
}
