MODULE_NAME=vlc.py

all: $(MODULE_NAME)

$(MODULE_NAME): generate.py header.py override.py ../../include/vlc/*.h
	./generate.py ../../include/vlc/*.h > $@

doc: $(MODULE_NAME)
	-epydoc -v -o doc $<

clean:
	-$(RM) $(MODULE_NAME)
