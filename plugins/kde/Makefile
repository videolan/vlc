MOC_SOURCES = kde_interface.moc.cpp kde_slider.moc.cpp kde_disc.moc.cpp kde_net.moc.cpp kde_menu.moc.cpp

kde_SOURCES = kde.cpp kde_interface.cpp kde_slider.cpp kde_disc.cpp kde_net.cpp kde_menu.cpp $(MOC_SOURCES)

$(MOC_SOURCES): %.moc.cpp: %.h
	$(MOC) $< -o $@
