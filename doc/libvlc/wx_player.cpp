// g++ wx_player.cpp `wx-config --libs` `wx-config --cxxflags` `pkg-config --cflags gtk+-2.0 libvlc` `pkg-config --libs gtk+-2.0 libvlc` -o wx_player

/* License WTFPL http://sam.zoy.org/wtfpl/ */
/* Written by Vincent Schüßler */

#include <wx/wx.h>
#include <wx/filename.h>
#include <vlc/vlc.h>
#include <climits>

#ifdef __WXGTK__
    #include <gdk/gdkx.h>
    #include <gtk/gtk.h>
    #include <wx/gtk/win_gtk.h>
    #define GET_XID(window) GDK_WINDOW_XWINDOW(GTK_PIZZA(window->m_wxwindow)->bin_window)
#endif

#define myID_PLAYPAUSE wxID_HIGHEST+1
#define myID_STOP wxID_HIGHEST+2
#define myID_TIMELINE wxID_HIGHEST+3
#define myID_VOLUME wxID_HIGHEST+4

#define TIMELINE_MAX (INT_MAX-9)
#define VOLUME_MAX 100

DECLARE_EVENT_TYPE(vlcEVT_END, -1)
DECLARE_EVENT_TYPE(vlcEVT_POS, -1)
DEFINE_EVENT_TYPE(vlcEVT_END)
DEFINE_EVENT_TYPE(vlcEVT_POS)

void OnPositionChanged_VLC(const libvlc_event_t *event, void *data);
void OnEndReached_VLC(const libvlc_event_t *event, void *data);

class MainWindow : public wxFrame {
    public:
        MainWindow(const wxString& title);
        ~MainWindow();

    private:
        void initVLC();

        void OnOpen(wxCommandEvent& event);
        void OnPlayPause(wxCommandEvent& event);
        void OnStop(wxCommandEvent& event);
        void OnPositionChanged_USR(wxCommandEvent& event);
        void OnPositionChanged_VLC(wxCommandEvent& event);
        void OnEndReached_VLC(wxCommandEvent& event);
        void OnVolumeChanged(wxCommandEvent& event);
        void OnVolumeClicked(wxMouseEvent& event);
        void OnTimelineClicked(wxMouseEvent& event);

        void play();
        void pause();
        void stop();
        void setTimeline(float value);
        void connectTimeline();

        wxButton *playpause_button;
        wxButton *stop_button;
        wxSlider *timeline;
        wxSlider *volume_slider;
        wxWindow *player_widget;

        libvlc_media_player_t *media_player;
        libvlc_instance_t *vlc_inst;
        libvlc_event_manager_t *vlc_evt_man;
};

MainWindow *mainWindow;

MainWindow::MainWindow(const wxString& title) : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition) {
    // setup menubar
    wxMenuBar *menubar;
    wxMenu *file;
    menubar = new wxMenuBar;
    file = new wxMenu;
    file->Append(wxID_OPEN, wxT("&Open"));
    menubar->Append(file, wxT("&File"));
    SetMenuBar(menubar);
    Connect(wxID_OPEN, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnOpen));

    // setup vbox
    wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(vbox);

    //setup player widget
    player_widget = new wxWindow(this, wxID_ANY);
    player_widget->SetBackgroundColour(wxColour(wxT("black")));
    vbox->Add(player_widget, 1, wxEXPAND | wxALIGN_TOP);

    //setup timeline slider
    timeline = new wxSlider(this, myID_TIMELINE, 0, 0, TIMELINE_MAX);
    timeline->Enable(false);
    vbox->Add(timeline, 0, wxEXPAND);
    connectTimeline();
    timeline->Connect(myID_TIMELINE, wxEVT_LEFT_UP, wxMouseEventHandler(MainWindow::OnTimelineClicked));

    //setup control panel
    wxPanel *controlPanel = new wxPanel(this, wxID_ANY);

    //setup hbox
    wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);
    controlPanel->SetSizer(hbox);
    vbox->Add(controlPanel, 0, wxEXPAND);

    //setup controls
    playpause_button = new wxButton(controlPanel, myID_PLAYPAUSE, wxT("Play"));
    stop_button = new wxButton(controlPanel, myID_STOP, wxT("Stop"));
    volume_slider = new wxSlider(controlPanel, myID_VOLUME, VOLUME_MAX, 0, VOLUME_MAX, wxDefaultPosition, wxSize(100, -1));
    playpause_button->Enable(false);
    stop_button->Enable(false);
    hbox->Add(playpause_button);
    hbox->Add(stop_button);
    hbox->AddStretchSpacer();
    hbox->Add(volume_slider);
    Connect(myID_PLAYPAUSE, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MainWindow::OnPlayPause));
    Connect(myID_STOP, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MainWindow::OnStop));
    Connect(myID_VOLUME, wxEVT_COMMAND_SLIDER_UPDATED, wxCommandEventHandler(MainWindow::OnVolumeChanged));
    volume_slider->Connect(myID_VOLUME, wxEVT_LEFT_UP, wxMouseEventHandler(MainWindow::OnVolumeClicked));

    //setup vlc
    vlc_inst = libvlc_new(0, NULL);
    media_player = libvlc_media_player_new(vlc_inst);
    vlc_evt_man = libvlc_media_player_event_manager(media_player);
    libvlc_event_attach(vlc_evt_man, libvlc_MediaPlayerEndReached, ::OnEndReached_VLC, NULL);
    libvlc_event_attach(vlc_evt_man, libvlc_MediaPlayerPositionChanged, ::OnPositionChanged_VLC, NULL);
    Connect(wxID_ANY, vlcEVT_END, wxCommandEventHandler(MainWindow::OnEndReached_VLC));
    Connect(wxID_ANY, vlcEVT_POS, wxCommandEventHandler(MainWindow::OnPositionChanged_VLC));

    Show(true);
    initVLC();
}

MainWindow::~MainWindow() {
    libvlc_media_player_release(media_player);
    libvlc_release(vlc_inst);
}

void MainWindow::initVLC() {
    #ifdef __WXGTK__
        libvlc_media_player_set_xwindow(media_player, GET_XID(this->player_widget));
    #else
        libvlc_media_player_set_hwnd(media_player, this->player_widget->GetHandle());
    #endif
}

void MainWindow::OnOpen(wxCommandEvent& event) {
    wxFileDialog openFileDialog(this, wxT("Choose File"));

    if (openFileDialog.ShowModal() == wxID_CANCEL) {
        return;
    }
    else {
        libvlc_media_t *media;
        wxFileName filename = wxFileName::FileName(openFileDialog.GetPath());
        filename.MakeRelativeTo();
        media = libvlc_media_new_path(vlc_inst, filename.GetFullPath().mb_str());
        libvlc_media_player_set_media(media_player, media);
        play();
        libvlc_media_release(media);
    }
}

void MainWindow::OnPlayPause(wxCommandEvent& event) {
    if(libvlc_media_player_is_playing(media_player) == 1) {
        pause();
    }
    else {
        play();
    }
}

void MainWindow::OnStop(wxCommandEvent& event) {
    stop();
}

void MainWindow::OnPositionChanged_USR(wxCommandEvent& event) {
    libvlc_media_player_set_position(media_player, (float) event.GetInt() / (float) TIMELINE_MAX);
}

void MainWindow::OnPositionChanged_VLC(wxCommandEvent& event) {
    float factor = libvlc_media_player_get_position(media_player);
    setTimeline(factor);
}

void MainWindow::OnEndReached_VLC(wxCommandEvent& event) {
    stop();
}

void MainWindow::OnVolumeChanged(wxCommandEvent& event) {
    libvlc_audio_set_volume(media_player, volume_slider->GetValue());
}

void MainWindow::OnVolumeClicked(wxMouseEvent& event) {
    wxSize size = mainWindow->volume_slider->GetSize();
    float position = (float) event.GetX() / (float) size.GetWidth();
    mainWindow->volume_slider->SetValue(position*VOLUME_MAX);
    libvlc_audio_set_volume(mainWindow->media_player, position*VOLUME_MAX);
    event.Skip();
}

void MainWindow::OnTimelineClicked(wxMouseEvent& event) {
    wxSize size = mainWindow->timeline->GetSize();
    float position = (float) event.GetX() / (float) size.GetWidth();
    libvlc_media_player_set_position(mainWindow->media_player, position);
    mainWindow->setTimeline(position);
    event.Skip();
}

void MainWindow::play() {
    libvlc_media_player_play(media_player);
    playpause_button->SetLabel(wxT("Pause"));
    playpause_button->Enable(true);
    stop_button->Enable(true);
    timeline->Enable(true);
}

void MainWindow::pause() {
    libvlc_media_player_pause(media_player);
    playpause_button->SetLabel(wxT("Play"));
}

void MainWindow::stop() {
    pause();
    libvlc_media_player_stop(media_player);
    stop_button->Enable(false);
    setTimeline(0.0);
    timeline->Enable(false);
}

void MainWindow::setTimeline(float value) {
    if(value < 0.0) value = 0.0;
    if(value > 1.0) value = 1.0;
    Disconnect(myID_TIMELINE);
    timeline->SetValue((int) (value * TIMELINE_MAX));
    connectTimeline();
}

void MainWindow::connectTimeline() {
    Connect(myID_TIMELINE, wxEVT_COMMAND_SLIDER_UPDATED, wxCommandEventHandler(MainWindow::OnPositionChanged_USR));
}

class MyApp : public wxApp {
    public:
        virtual bool OnInit();
};

void OnPositionChanged_VLC(const libvlc_event_t *event, void *data) {
    wxCommandEvent evt(vlcEVT_POS, wxID_ANY);
    mainWindow->GetEventHandler()->AddPendingEvent(evt);
}

void OnEndReached_VLC(const libvlc_event_t *event, void *data) {
    wxCommandEvent evt(vlcEVT_END, wxID_ANY);
    mainWindow->GetEventHandler()->AddPendingEvent(evt);
}

bool MyApp::OnInit() {
    mainWindow = new MainWindow(wxT("wxWidgets libVLC demo"));
    return true;
}

IMPLEMENT_APP(MyApp)
