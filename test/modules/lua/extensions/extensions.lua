function descriptor()
  return {
    title = "test",
    version = "0.0.1",
    author = "VideoLAN",
    shortdesc = "Test example",
    description = "Test description",
    capabilities = {
        "input-listener",
        "meta-listener",
        "playing-listener",
     }
  }
end

function activate()
  vlc.msg.dbg("Activate")
end

function close()
  vlc.msg.dbg("Close")
end

function deactivate()
  vlc.msg.dbg("Deactivate")
end

function input_changed()
  vlc.msg.dbg("Input changed")
end

function playing_changed()
  vlc.msg.dbg("Playing changed")
end

function meta_changed()
  vlc.msg.dbg("Meta changed")
end
