#include "place.hpp"
#include "xdg_dirs.hpp"

place::place(std::string label_str, std::string path_str) {
	file_path = path_str;
	get_style_context()->add_class("place");
	append(image);
	append(label);

	image.set_margin_end(5);
	label.set_text(label_str);

	if (xdg_dirs[path_str] != "")
		image.set_from_icon_name(xdg_dirs[path_str]);
	else
		image.set_from_icon_name("folder-symbolic");
}
