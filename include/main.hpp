#pragma once
#include <gtkmm/application.h>

class frog;

inline Glib::RefPtr<Gtk::Application> app;
inline frog* win;
inline Glib::ustring start_path;
inline bool desktop_mode;
