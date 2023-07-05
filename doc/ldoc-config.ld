project = "VLC"
title = "VLC Lua scripting"
description = "The VLC Lua scripting reference documentation"
format = 'markdown'
style = "!new"
merge = true
no_space_before_args = true
wrap = true
boilerplate = true
file = {
	'../modules/lua/libs/configuration.c',
	'../modules/lua/libs/dialog.c',
	'../modules/lua/libs/strings.c',
}
kind_names={
	topic='Manuals',
}

topics = {
	'../share/lua/extensions/Extensions.md',
}
