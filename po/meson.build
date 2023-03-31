i18n = import('i18n')

add_project_arguments(f'-DGETTEXT_PACKAGE="@vlc_package_name@"',
	language: ['c', 'cpp', 'objc'])
i18n.gettext(vlc_package_name,
	args: [
		'--keyword=_',
		'--keyword=N_',
		'--keyword=_NS',
		'--keyword=qtr',
		'--keyword=Q_',
		'--language=C++',
		'--keyword=vlc_ngettext:1,2',
		'--keyword=vlc_pgettext:1c,2',
		'--keyword=_PNS:1c,2',
		'--add-comments=xgettext:',
		'--from-code=UTF-8',
	])
