#include "place.hpp"

#include <iostream>

place::place(std::string label_str, std::string path_str) {
	file_path = path_str;
	get_style_context()->add_class("place");
	append(image);
	append(label);

	image.set_from_icon_name("folder-symbolic");
	image.set_margin_end(5);
	label.set_text(label_str);
}
