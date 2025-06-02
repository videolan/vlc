function parse_json(str)
    if not str or str == "" then return nil end
    local json = require("dkjson")
    local ok, data = pcall(json.decode, str)
    if ok then
        return data
    else
        vlc.msg.err("Error parsing JSON: " .. data)
        return nil
    end
end

function get_json(url)
    vlc.msg.info("Fetching JSON from " .. url)
    
    local proxy = "https://corsproxy.io/?"
    local proxy_url = proxy .. url

    -- Open stream
    local stream = vlc.stream(proxy_url)
    local data = ""
    local line = ""

    if not stream then 
        vlc.msg.err("Failed to connect using proxy")
        return false 
    end

    -- Read data
    while true do
        line = stream:readline()
        if not line then break end
        data = data .. line
    end

    -- Validate and parse JSON
    local json_data = parse_json(data)
    if not json_data then
        vlc.msg.err("JSON parsing error")
        return false
    end

    return json_data
end

function get_stream_info(url)
    vlc.msg.info("Getting stream info from: " .. url)
    local stream = vlc.stream(url)
    if not stream then
        return { { path="", name="Error fetching streams" } }
    end

    local items = {}
    local name = "Source"

    while true do
        local line = stream:readline()
        if not line then break end
        
        if string.find(line, "^#EXT%-X%-MEDIA:") then
            local n = line:match('NAME="([^"]+)"')
            if n then name = n end
        elseif string.find(line, "^http") then
            table.insert(items, {
                path = line,
                name = name
            })
        end
    end

    return items
end

function string.starts(haystack, needle)
    return string.sub(haystack, 1, string.len(needle)) == needle
end

function probe()
    local valid_access = (vlc.access == "http" or vlc.access == "https")
    local kick_url = string.starts(vlc.path, "kick.com/") or 
                     string.starts(vlc.path, "www.kick.com/") or
                     string.starts(vlc.path, "m.kick.com/")
    return valid_access and kick_url
end

function parse()
    local path = vlc.path
    vlc.msg.info("Processing Kick URL: " .. path)
    
    -- Extract channel name
    local channel = path:match("kick%.com/([%w_]+)") or 
                    path:match("www%.kick%.com/([%w_]+)") or
                    path:match("m%.kick%.com/([%w_]+)")
    
    if not channel then
        vlc.msg.err("Invalid URL format: " .. path)
        return { { path="", name="Error: Invalid URL format" } }
    end
    
    vlc.msg.info("Detected channel: " .. channel)
    local api_url = "https://kick.com/api/v1/channels/" .. channel
    
    -- Get channel data
    local data = get_json(api_url)
    
    if not data or not data.playback_url or data.playback_url == "" then
        local status = (data and data.livestream and data.livestream.is_live) and "offline" or "inactive"
        return { { 
            path="", 
            name="Channel " .. status .. ": " .. channel,
            arturl = data.user and data.user.profile_pic or nil
        } }
    end
    
    -- Create base item
    local item = {
        path = data.playback_url,
        name = "Kick channel: " .. channel,
        options = {"http-user-agent=Mozilla/5.0"}
    }
    
    -- Get quality variants
    local streams = get_stream_info(item.path)
    if #streams == 0 then
        return { item }
    end
    
    
    local result = {}
    for _, stream in ipairs(streams) do
        table.insert(result, {
            path = stream.path,
            name = item.name .. " [" .. stream.name .. "]",
            options = item.options,
            arturl = item.arturl
        })
    end
    
    return result
end
