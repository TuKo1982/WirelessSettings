# Makefile pour WirelessSettings
# Compatible GCC 2.95 / AmigaOS

CC = gcc
# -Wno-implicit-int supprime les warnings "type defaults to int" des headers MUI
CFLAGS = -noixemul -s -O2 -Wall -Wno-implicit-int
LIBS = -lamiga
TARGET = WirelessSettings
SOURCE = WirelessSettings.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)
	@echo "Compilation terminée : $(TARGET)"

clean:
	rm -f $(TARGET)

install: $(TARGET)
	copy $(TARGET) SYS:Prefs/

.PHONY: all clean install
