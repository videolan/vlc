qt_SOURCES = qt.cpp intf_qt.cpp
MOC_QT = intf_qt.moc

EXTRA_DEP = $(MOC_QT)

$(MOC_QT): %.moc: %.cpp
	$(MOC) -i $< -o $@
