/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
pragma Singleton
import QtQuick 2.11
import org.videolan.vlc 0.1

QtObject {
    id: vlc_style

    readonly property real scale: MainCtx.intfScaleFactor

    readonly property FontMetrics fontMetrics_xxsmall  : FontMetrics { font.pixelSize: dp(6, scale);  }
    readonly property FontMetrics fontMetrics_xsmall   : FontMetrics { font.pixelSize: dp(8, scale);  }
    readonly property FontMetrics fontMetrics_small    : FontMetrics { font.pixelSize: dp(10, scale); }
    readonly property FontMetrics fontMetrics_normal   : FontMetrics { font.pixelSize: dp(12, scale); }
    readonly property FontMetrics fontMetrics_large    : FontMetrics { font.pixelSize: dp(14, scale); }
    readonly property FontMetrics fontMetrics_xlarge   : FontMetrics { font.pixelSize: dp(16, scale); }
    readonly property FontMetrics fontMetrics_xxlarge  : FontMetrics { font.pixelSize: dp(20, scale); }
    readonly property FontMetrics fontMetrics_xxxlarge : FontMetrics { font.pixelSize: dp(24, scale); }

    property alias self: vlc_style

    readonly property SystemPalette theme:  SystemPalette {
        source: MainCtx.colorScheme.scheme
        ctx: MainCtx
    }

    readonly property VLCColors colors: VLCColors {
        palette: vlc_style.theme
    }

    // When trying to force night/dark theme colors for items,
    // this can be used:
    readonly property VLCColors nightColors: VLCColors {
        palette: SystemPalette {
            source: ColorSchemeModel.Night
            ctx: MainCtx
        }
    }

    // Sizes
    readonly property double margin_xxxsmall: dp(2, scale);
    readonly property double margin_xxsmall: dp(4, scale);
    readonly property double margin_xsmall: dp(8, scale);
    readonly property double margin_small: dp(12, scale);
    readonly property double margin_normal: dp(16, scale);
    readonly property double margin_large: dp(24, scale);
    readonly property double margin_xlarge: dp(32, scale);
    readonly property double margin_xxlarge: dp(36, scale);

    // Borders
    readonly property int border: dp(1, scale)
    readonly property int focus_border: border

    readonly property int fontSize_xsmall: fontMetrics_xsmall.font.pixelSize
    readonly property int fontSize_small:  fontMetrics_small.font.pixelSize
    readonly property int fontSize_normal: fontMetrics_normal.font.pixelSize
    readonly property int fontSize_large:  fontMetrics_large.font.pixelSize
    readonly property int fontSize_xlarge: fontMetrics_xlarge.font.pixelSize
    readonly property int fontSize_xxlarge: fontMetrics_xxlarge.font.pixelSize
    readonly property int fontSize_xxxlarge: fontMetrics_xxxlarge.font.pixelSize

    readonly property int fontHeight_xsmall: Math.ceil(fontMetrics_xsmall.lineSpacing)
    readonly property int fontHeight_small:  Math.ceil(fontMetrics_small.lineSpacing)
    readonly property int fontHeight_normal: Math.ceil(fontMetrics_normal.lineSpacing)
    readonly property int fontHeight_large:  Math.ceil(fontMetrics_large.lineSpacing)
    readonly property int fontHeight_xlarge: Math.ceil(fontMetrics_xlarge.lineSpacing)
    readonly property int fontHeight_xxlarge: Math.ceil(fontMetrics_xxlarge.lineSpacing)
    readonly property int fontHeight_xxxlarge: Math.ceil(fontMetrics_xxxlarge.lineSpacing)


    readonly property int heightAlbumCover_xsmall: dp(32, scale);
    readonly property int heightAlbumCover_small: dp(64, scale);
    readonly property int heightAlbumCover_normal: dp(128, scale);
    readonly property int heightAlbumCover_large: dp(255, scale);
    readonly property int heightAlbumCover_xlarge: dp(512, scale);

    readonly property int listAlbumCover_height: dp(32, scale)
    readonly property int listAlbumCover_width: listAlbumCover_height * 16.0/9
    readonly property int listAlbumCover_radius: dp(3, scale)
    readonly property int trackListAlbumCover_width: dp(32, scale)
    readonly property int trackListAlbumCover_heigth: dp(32, scale)
    readonly property int trackListAlbumCover_radius: dp(2, scale)

    readonly property int tableCoverRow_height: Math.max(listAlbumCover_height, fontHeight_normal) + margin_xsmall * 2
    readonly property int tableRow_height: fontHeight_normal + margin_small * 2

    readonly property int icon_xsmall: dp(8, scale);
    readonly property int icon_small: dp(12, scale);
    readonly property int icon_normal: dp(24, scale);
    readonly property int icon_medium: dp(48, scale);
    readonly property int icon_large: dp(64, scale);
    readonly property int icon_xlarge: dp(128, scale);

    readonly property int icon_topbar: icon_normal
    readonly property int icon_toolbar: icon_normal
    readonly property int icon_audioPlayerButton: dp(32, scale)
    readonly property int icon_playlist: icon_normal
    readonly property int icon_track: icon_normal
    readonly property int icon_tableHeader: icon_normal
    readonly property int icon_playlistHeader: icon_normal
    readonly property int icon_banner: dp(28, scale)
    readonly property int icon_play: dp(28, scale)
    readonly property int icon_addressBar: icon_normal
    readonly property int icon_actionButton: icon_normal
    readonly property int icon_PIP: icon_normal
    readonly property int icon_CSD: icon_small

    readonly property int play_cover_small: dp(24, scale)
    readonly property int play_cover_normal: dp(48, scale)

    readonly property int cover_xxsmall: dp(32, scale);
    readonly property int cover_xsmall: dp(64, scale);
    readonly property int cover_small: dp(96, scale);
    readonly property int cover_normal: dp(128, scale);
    readonly property int cover_large: dp(160, scale);
    readonly property int cover_xlarge: dp(192, scale);

    readonly property int heightBar_xxxsmall: dp(2, scale);
    readonly property int heightBar_xxsmall: dp(4, scale);
    readonly property int heightBar_xsmall: dp(8, scale);
    readonly property int heightBar_small: dp(16, scale);
    readonly property int heightBar_normal: dp(32, scale);
    readonly property int heightBar_medium: dp(48, scale);
    readonly property int heightBar_large: dp(64, scale);
    readonly property int heightBar_xlarge: dp(128, scale);
    readonly property int heightBar_xxlarge: dp(256, scale);

    readonly property int minWindowWidth: dp(500, scale);
    readonly property int maxWidthPlaylist: dp(400, scale);
    readonly property int defaultWidthPlaylist: dp(300, scale);
    readonly property int closedWidthPlaylist: dp(20, scale);

    readonly property int widthSearchInput: dp(200, scale);
    readonly property int widthSortBox: dp(150, scale);
    readonly property int widthTeletext: dp(280, scale);
    readonly property int widthExtendedSpacer: dp(128, scale);
    readonly property int heightInput: dp(22, scale);

    readonly property int scrollbarWidth: dp(4, scale);
    readonly property int scrollbarHeight: dp(100, scale);

    readonly property real network_normal: dp(100, scale)

    readonly property int expandAlbumTracksHeight: dp(200, scale)

    //combobox
    readonly property int combobox_width_small: dp(64, scale)
    readonly property int combobox_width_normal: dp(96, scale)
    readonly property int combobox_width_large: dp(128, scale)

    readonly property int combobox_height_small: dp(16, scale)
    readonly property int combobox_height_normal: dp(24, scale)
    readonly property int combobox_height_large: dp(30, scale)

    //button
    readonly property int button_width_small: dp(64, scale)
    readonly property int button_width_normal: dp(96, scale)
    readonly property int button_width_large: dp(128, scale)

    readonly property int checkButton_width: dp(56, scale)
    readonly property int checkButton_height: dp(32, scale)

    readonly property int checkButton_margins: dp(4, scale)
    readonly property int checkButton_handle_margins: dp(2, scale)

    readonly property int table_section_width: dp(32, scale)
    readonly property int table_section_text_margin: dp(10, scale)

    readonly property int gridCover_network_width: colWidth(1)
    readonly property int gridCover_network_height: gridCover_network_width
    readonly property int gridCover_network_border: dp(3, scale)

    readonly property int gridCover_music_width: colWidth(1)
    readonly property int gridCover_music_height: gridCover_music_width
    readonly property int gridCover_music_border: dp(3, scale)

    readonly property int gridCover_video_width: colWidth(2)
    readonly property int gridCover_video_height: ( gridCover_video_width * 10.0 ) / 16
    readonly property int gridCover_video_border: dp(4, scale)

    readonly property int gridCover_radius: dp(4, scale)

    readonly property int expandCover_music_height: dp(171, scale)
    readonly property int expandCover_music_width: dp(171, scale)
    readonly property int expandCover_music_radius: gridCover_radius
    readonly property int expandDelegate_border: dp(1, scale)

    readonly property int artistGridCover_radius: dp(90, scale)

    //GridItem
    readonly property int gridItem_network_width: VLCStyle.gridCover_network_width
    readonly property int gridItem_network_height: VLCStyle.gridCover_network_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal

    readonly property int gridItem_music_width: VLCStyle.gridCover_music_width
    readonly property int gridItem_music_height: VLCStyle.gridCover_music_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal + VLCStyle.margin_xsmall + VLCStyle.fontHeight_small

    readonly property int gridItem_video_width: VLCStyle.gridCover_video_width
    readonly property int gridItem_video_height: VLCStyle.gridCover_video_height + VLCStyle.margin_xxsmall + VLCStyle.fontHeight_normal + VLCStyle.fontHeight_normal

    readonly property int gridItemSelectedBorder: dp(8, scale)

    readonly property int gridItem_newIndicator: dp(8, scale)

    readonly property int column_width: dp(114, scale)

    // NOTE: This property should be applied on ExpandGridView and TableView. We should provision
    //       enough space to fit the TableView section labels and 'contextButton'.
    readonly property int column_margin: dp(32, scale)
    readonly property int column_spacing: column_margin

    readonly property int table_cover_border: dp(2, scale)

    readonly property int artistBanner_height: dp(200, scale)

    //global application size, updated by the root widget
    property int appWidth: 0
    property int appHeight: 0

    readonly property int smallWidth: dp(600, scale)
    readonly property bool isScreenSmall: appWidth <= smallWidth

    //global application margin "safe area"
    readonly property int applicationHorizontalMargin: 0
    readonly property int applicationVerticalMargin: 0

    readonly property int globalToolbar_height: dp(40, scale)
    readonly property int localToolbar_height: dp(48, scale)

    readonly property int bannerTabButton_width_small: icon_banner
    readonly property int bannerTabButton_width_large: column_width

    readonly property int bannerButton_height: icon_banner
    readonly property int bannerButton_width: icon_banner

    // Drag and drop

    readonly property int dragDelta: dp(12, scale)

    // durations. Values are aligned on Kirigami

    //should be used for animation that benefits from a longer animation than duration_long
    readonly property int duration_veryLong: 400

    //should be used for longer animation (opening/closing panes & dialogs)
    readonly property int duration_long: 200

    //should be used for short animations (hovering, accuenting UI event)
    readonly property int duration_short: 100

    //should be used for near instant animations
    readonly property int duration_veryShort: 50

    /* human time reaction, how much time before the user should be informed that something
     * is going on, or before something should be automatically automated,
     * this should not be used for animations
     *
     * Some examples:
     *
     * - When the user types text in a search field, wait no longer than this duration after
     *   the user completes typing before starting the search
     * - When loading data which would commonly arrive rapidly enough to not require interaction,
     *   wait this long before showing a spinner
     */
    readonly property int duration_humanMoment: 2000

    //timing before showing up a tooltip
    readonly property int delayToolTipAppear: 700

    //timing for the progressbar/scanbar bouncing animation, explicitly very long
    readonly property int durationSliderBouncing: 2000

    //default arts
    readonly property url noArtAlbum: "qrc:///placeholder/noart_album.svg";
    readonly property url noArtArtist: "qrc:///placeholder/noart_artist.svg";
    readonly property url noArtArtistSmall: "qrc:///placeholder/noart_artist_small.svg";
    readonly property url noArtAlbumCover: "qrc:///placeholder/noart_albumCover.svg";
    readonly property url noArtArtistCover: "qrc:///placeholder/noart_artistCover.svg";
    readonly property url noArtVideoCover: "qrc:///placeholder/noart_videoCover.svg";

    // Play shadow
    readonly property url playShadow: "qrc:///misc/play_shadow.png";

    // New indicator
    readonly property url newIndicator: "qrc:///misc/new_indicator.svg";

    // Player controlbar
    readonly property int maxControlbarControlHeight: dp(64, scale)

    readonly property var dp: MainCtx.dp

    function colWidth(nb) {
      return nb * VLCStyle.column_width + ( nb - 1 ) * VLCStyle.column_spacing;
    }

    //Returns the number columns fitting in given width
    function gridColumnsForWidth(width) {
        return Math.floor((width + column_spacing) / (column_width + column_spacing))
    }

}
