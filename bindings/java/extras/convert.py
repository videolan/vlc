#!/usr/bin/python
"""
vlc to jna almost-automatized interface converter.

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details. 

"""

import sys

file = open('libvlc.h', 'r')

parameter_parsing = False
prototype_parsing = False

types_map = [
	["const ", ""],
	["VLC_PUBLIC_API ", ""],
	["char**", "String[] "],
	["char **" , "String[] "],
	["char*" , "String "],
	["char *" , "String "],
	["libvlc_instance_t *", "LibVlcInstance "],
	["libvlc_exception_t *", "libvlc_exception_t "],
	["libvlc_log_t *", "LibVlcLog "],
	["libvlc_log_iterator_t *", "LibVlcLogIterator "],
	["libvlc_log_message_t *", "libvlc_log_message_t "],
	["unsigned", "int"],
]

def convert_prototype(proto, parameters):
	#print proto
	#print parameters
	tokens = proto.split(",")
	last = tokens.pop().split(")")
	res = ''
	for i in tokens:
		param = parameters.pop(0)
		if i.find(param)==-1:
			res += i + " " + param + ","
		else:
			res += i + " ,"
	if len(parameters):
		param = parameters.pop(0)
		if last[0].find(param)==-1:
			res += last[0] + " " + param + ")" + last[1]
		else:
			res += last[0] + ")" + last[1]

	for k,v in types_map:
		res = res.replace(k,v)
	print res

for line in file.readlines():
	if line.find("/**")!=-1:
		parameters = []
		parameter_parsing = True

	if line.find("VLC_PUBLIC_API")!=-1:
		if not parameters:
			continue
		prototype_line = ''
		prototype_parsing = True

	if parameter_parsing:
		param_index = line.find("\param ")
		if not param_index==-1:
			parameter = line.split()[2]
			parameters.append(parameter)
		if line.find("*/")!=-1:
			parameter_parsing = False

	if prototype_parsing:
		prototype_line += line.strip()
		if line.find(");")!=-1:
			prototype_parsing = False
			convert_prototype(prototype_line, parameters)
			parameters = None
		continue

	#sys.stdout.write(line)

# vi:ts=4
