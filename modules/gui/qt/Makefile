qt_SOURCES = qt.cpp intf.cpp
MOC_QT = intf.moc

EXTRA_DEP = $(MOC_QT)

$(MOC_QT): %.moc: %.cpp
	$(MOC) -i $< -o $@

