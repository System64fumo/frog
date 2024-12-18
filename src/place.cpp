#include "place.hpp"
#include "xdg_dirs.hpp"

place::place(std::string label_str, std::string path_str, std::string custom_icon, const std::optional<disk_manager::partition>& part) {
	file_path = path_str;
	get_style_context()->add_class("place");
	append(image);

	set_size_request(-1, 30);
	image.set_margin_end(7);
	label.set_text(label_str);
	label.set_halign(Gtk::Align::START);

	if (part) {
		is_disk = true;
		this->part = *part;

		if (part->used_bytes != 0) {
			const double used_percentage = (double)part->used_bytes / (double)part->total_bytes;
			Gtk::Box *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
			progressbar_size.set_fraction(used_percentage);
			progressbar_size.set_margin_top(5);
			append(*box);
			box->set_hexpand(true);
			box->append(label);
			box->append(progressbar_size);
		}
		else {
			append(label);
			file_path = "";
		}
	}
	else
		append(label);

	if (custom_icon != "") {
		image.set_from_icon_name(custom_icon);
		return;
	}

	if (xdg_dirs[path_str] != "")
		image.set_from_icon_name(xdg_dirs[path_str]);
	else
		image.set_from_icon_name("folder-symbolic");
}
