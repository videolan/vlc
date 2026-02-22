# VLC plugin extension

To package independent plugins for VLC you can create an extension org.videolan.VLC.Plugin.myplugin.

The files in your extension will be available in /app/share/vlc/extra/myplugin.

The lib folder will automatically be added in the runtime LD search paths so you can link your plugin to whatever is there.

To add a VLC plugin put the .so inside a plugins directory at the root of your extension. It will then be available to VLC.

All the .sh files available at the root of the extension will be sourced before launching VLC. You can then mess around with environment variables, and also modify VLC_ARGS which will be prepend to the args passed by the user.

