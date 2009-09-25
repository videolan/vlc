-- Parser script from Rockbox FM radio presets
-- See http://www.rockbox.org/wiki/FmPresets

function probe()
	if not string.match( vlc.path, ".fmr$" ) then return false end
	local line = vlc.peek(256)
	vlc.msg.err(line)
	local freq = tonumber(string.match( line, "(%d*):" ))
	if not freq then return false end
	return freq > 80000000	and freq < 110000000
end

function parse()
	vlc.msg.err("test")
	local p = {}
	while true do
		line = vlc.readline()
		if not line then break end
	vlc.msg.err(line)
		for freq, name in string.gmatch( line, "(%d*):(.*)" ) do
			vlc.msg.info(freq)
			table.insert( p, { path = "v4l2c:///dev/radio0:tuner-frequency="..freq, name = name } )
		end
	end
vlc.msg.err("test test")
	return p
end
