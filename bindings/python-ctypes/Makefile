MODULE_NAME=vlc.py

all: $(MODULE_NAME)

$(MODULE_NAME): generate.py header.py footer.py override.py ../../include/vlc/*.h
	python generate.py ../../include/vlc/*.h > $@

doc: $(MODULE_NAME)
	-epydoc -v -o doc $<

test: $(MODULE_NAME)
	python test.py

check: $(MODULE_NAME)
	-pyflakes $<
	-pylint $<

clean:
	-$(RM) $(MODULE_NAME)
