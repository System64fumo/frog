#include <gtkmm/dialog.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/centerbox.h>
#include <glibmm/dispatcher.h>

class file_entry;

class properties_dialog : public Gtk::Dialog {
	public:
		properties_dialog(file_entry* f_entry);

	private:
		Glib::Dispatcher dispatcher;

		Gtk::Box* box_main;
		Gtk::Box box_preview;
		Gtk::Image image_preview;
		Gtk::Label label_preview;

		// General
		Gtk::CenterBox* centerbox_type;
		Gtk::CenterBox* centerbox_size;
		Gtk::CenterBox* centerbox_content;

		// Time
		Gtk::CenterBox* centerbox_created;
		Gtk::CenterBox* centerbox_modified;
		Gtk::CenterBox* centerbox_accessed;

		// Permissions
		// TODO: Implement

		Gtk::Box* create_card();
		Gtk::CenterBox* add_property(Gtk::Box* container, Gtk::Widget* name, Gtk::Widget* value);
		Gtk::CenterBox* add_property_text(Gtk::Box* container, std::string title, std::string content = "Loading..");
};