MOC_SOURCES = interface.moc.cpp slider.moc.cpp disc.moc.cpp net.moc.cpp menu.moc.cpp preferences.moc.cpp pluginsbox.moc.cpp QConfigItem.moc.cpp

kde_SOURCES = kde.cpp interface.cpp slider.cpp disc.cpp net.cpp menu.cpp preferences.cpp pluginsbox.cpp QConfigItem.cpp $(MOC_SOURCES)

$(MOC_SOURCES): %.moc.cpp: %.h
	$(MOC) $< -o $@
