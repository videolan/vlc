local _G = _G
module("custom",package.seeall)

local dialogs_cache = {}

function dialog_preload(name)
    if not dialogs_cache[name] then
        -- Cache the dialogs
        dialogs_cache[name] = process(http_dir.."/dialogs/"..name)
    end
end

function dialog(name)
    dialog_preload(name)
    dialogs_cache[name]()
end

function dialogs(...)
    for i=1,select("#",...) do
        dialog(select(i,...))
    end
end

_G.dialogs = dialogs
_G.vlm = vlc.vlm()
