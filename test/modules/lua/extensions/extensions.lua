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

function signal_test(event_name)
  vlc.msg.dbg("lua test event: " .. event_name)
  libvlc = vlc.object.libvlc()
  vlc.var.trigger_callback(libvlc, "test-lua-" .. event_name)
end

function activate()
  signal_test("activate")
end

function close()
  signal_test("close")
end

function deactivate()
  signal_test("deactivate")
end

function input_changed()
  signal_test("input-changed")
end

function playing_changed()
  signal_test("playing-changed")
end

function meta_changed()
  signal_test("meta-changed")
end
