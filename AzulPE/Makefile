PREFIX ?= x86_64-w64-mingw32-
CXX = $(PREFIX)g++
RC = $(PREFIX)windres
CXXFLAGS = -std=c++17 -Os -Wall -Wextra -Wpedantic -Wno-missing-field-initializers -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE -finput-charset=UTF-8 -fexec-charset=UTF-8
LIBS = -lcomctl32 -lmsimg32 -lgdi32 -lshell32 -ladvapi32 -luser32
SOURCES = src/main.cpp src/paint.cpp src/files.cpp src/dialogs.cpp src/explorer.cpp
OBJECTS = $(SOURCES:src/%.cpp=obj/%.o) obj/azul.res.o

.PHONY: all clean
all: AzulPE.exe

obj:
	mkdir -p $@

obj/%.o: src/%.cpp src/base.hpp | obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

obj/azul.res.o: azul.rc assets/azul.ico assets/azul.manifest assets/parede.dib assets/icones.bgra assets/Inter-Regular.otf assets/Inter-SemiBold.otf | obj
	$(RC) -i $< -O coff -o $@

AzulPE.exe: $(OBJECTS)
	$(CXX) -s -static -municode -mwindows -Wl,--subsystem,windows:6.01 -o $@ $(OBJECTS) $(LIBS)

clean:
	rm -rf obj AzulPE.exe
