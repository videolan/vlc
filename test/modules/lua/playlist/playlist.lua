local check_peek_read = "<html>"

function probe()
	vlc.msg.dbg("Access:" .. vlc.access)
	vlc.msg.dbg("Path:" .. vlc.path)
	local peeked = vlc.peek(6)
	return vlc.access == "mock" and vlc.path == "length=100" and peeked == check_peek_read
end

function parse()
	vlc.msg.dbg("parse is triggered ")
	local read_bytes = vlc.read(6)
	if read_bytes == check_peek_read then
		vlc.msg.dbg("read is triggered")
	end
	local line = vlc.readline()
	local title = string.match(line, "<title>(.-)</title>")
	if title ~= nil then
		vlc.msg.dbg("readline is triggered")
	end
	return { { name = title, path = vlc.access .. "://" .. vlc.path } }
end
