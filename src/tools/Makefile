
INC = -I../engine/include -I../engine/devices

system	:= $(shell uname -s)
ifeq ($(system), Darwin)
  lib =  -stdlib=libc++ -mmacosx-version-min=10.7 -F../../build/MacOS/Release/ -framework GUIDOEngine
  debuglib =  -stdlib=libc++ -mmacosx-version-min=10.7 -F../../build/MacOS/Debug/ -framework GUIDOEngine
  INC := $(INC)  -I../../platforms/macosx/src
else
ifeq ($(system), MINGW32_NT-6.1)
  lib =  ../../build/win32/Release/GUIDOEngine.lib
else
  lib = -lGUIDOEngine
  INC := $(INC)  -I../../platforms/linux/src $(shell pkg-config --cflags cairo)
endif
endif

TOOLS = guidogetversion guidogetpagecount guidogetvoicecount guidogettimemap guidogetpagemap guidogetsystemmap guidogetstaffmap guidogetvoicemap guido2svg guido2svgmap guido2abstract guido2proll guido2rproportional midi2proll midi2rproportional guido2midi guidogetmeter # guido2gmn

.PHONY: checkall

all: $(TOOLS)

guido2gmn : guido2gmn.cpp
	g++ guido2gmn.cpp  $(INC) $(lib) -o guido2gmn

guidogetversion : guidogetversion.cpp
	g++ guidogetversion.cpp -I../engine/include $(lib) -o guidogetversion

midi2proll : midi2proll.cpp
	g++ midi2proll.cpp $(INC) $(lib) -o midi2proll

guido2proll : guido2proll.cpp
	g++ guido2proll.cpp $(INC) $(lib) -o guido2proll

midi2rproportional : midi2rproportional.cpp
	g++ midi2rproportional.cpp $(INC) $(lib) -o midi2rproportional

guido2rproportional : guido2rproportional.cpp
	g++ guido2rproportional.cpp $(INC) $(lib) -o guido2rproportional

guido2midi : guido2midi.cpp
	g++ guido2midi.cpp $(INC) $(lib) -o guido2midi

midi2guido : midi2guido.cpp
	g++ midi2guido.cpp $(INC) $(lib) -o midi2guido

guido2svg : guido2svg.cpp
	g++ guido2svg.cpp $(INC) $(lib) -o guido2svg

guido2abstract : guido2abstract.cpp
	g++ guido2abstract.cpp $(INC) $(lib) -o guido2abstract

guido2svgmap : guido2svgmap.cpp
	g++ guido2svgmap.cpp $(INC) $(lib) -o guido2svgmap

guidogetpagecount : guidogetpagecount.cpp
	g++ guidogetpagecount.cpp $(INC) $(lib) -o guidogetpagecount

guidogetvoicecount : guidogetvoicecount.cpp
	g++ guidogetvoicecount.cpp $(INC) $(lib) -o guidogetvoicecount

guidogetmeter : guidogetmeter.cpp
	g++ guidogetmeter.cpp $(INC) $(lib) -o guidogetmeter

guidogettimemap : guidogettimemap.cpp
	g++ guidogettimemap.cpp $(INC) $(lib) -o guidogettimemap

guidogetpagemap : guidogetpagemap.cpp
	g++ guidogetpagemap.cpp $(INC) $(lib) -o guidogetpagemap

guidogetsystemmap : guidogetsystemmap.cpp
	g++ guidogetsystemmap.cpp $(INC) $(lib) -o guidogetsystemmap

guidogetstaffmap : guidogetstaffmap.cpp
	g++ guidogetstaffmap.cpp $(INC) $(lib) -o guidogetstaffmap

guidogetvoicemap : guidogetvoicemap.cpp
	g++ guidogetvoicemap.cpp $(INC) $(lib) -o guidogetvoicemap

install:
	cp $(TOOLS) $(HOME)/bin

clean:
	rm -f $(TOOLS)

