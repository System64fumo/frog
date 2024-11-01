#include "file.hpp"
#include "icons.hpp"
#include "xdg_dirs.hpp"

#include <gtkmm/dragsource.h>
#include <gtkmm/droptarget.h>
#include <gtkmm/stack.h>
#include <gtkmm/label.h>
#include <gtkmm/text.h>
#include <giomm/file.h>
#include <gtkmm/icontheme.h>
#include <glibmm/vectorutils.h>
#include <glibmm/convert.h>
#include <gstreamer-1.0/gst/gst.h>
#include <gst/video/video-info.h>
#include <thread>

file_entry::file_entry(const std::filesystem::directory_entry &entry) {
	path = entry.path();
	file_name = entry.path().filename().string();
	set_orientation(Gtk::Orientation::VERTICAL);

	append(image);
	image.set_pixel_size(icon_size);
	image.set_from_icon_name("content-loading-symbolic");

	// TODO: Add option to show/hide hidden files
	// TODO: Add option to enable/disable shadowing of hidden files
	if (file_name[0] == '.')
		image.set_opacity(0.75);

	append(label);
	label.set_text(file_name);
	label.set_halign(Gtk::Align::CENTER);
	auto stack = dynamic_cast<Gtk::Stack*>(label.get_children()[0]);
	auto ulabel = dynamic_cast<Gtk::Label*>(stack->get_children()[0]);
	auto text = dynamic_cast<Gtk::Text*>(stack->get_children()[1]);
	stack->set_hhomogeneous(false);
	ulabel->set_lines(2);
	ulabel->set_justify(Gtk::Justification::CENTER);
	ulabel->set_ellipsize(Pango::EllipsizeMode::END);
	ulabel->set_max_width_chars(10);
	ulabel->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
	text->set_propagate_text_width(true);
	text->set_max_width_chars(10);

	// Handle renaming
	label.property_editing().signal_changed().connect([&, entry]() {
		if (label.get_text() != file_name.c_str()) {
			std::string cur_path = entry.path().parent_path();
			std::string new_name = label.get_text();
			std::filesystem::rename(path, cur_path + "/" + new_name);
		}
	});

	// Figure out the correct icon
	file_size = entry.is_regular_file() ? entry.file_size() : 0;
	is_directory=entry.is_directory();

	// Set up drag and drop
	auto source = Gtk::DragSource::create();
	source->set_actions(Gdk::DragAction::MOVE);

	source->signal_prepare().connect([&](const double &x, const double &y) {
		// TODO: Replace C implementation with the proper C++ implementation
		// I have spent far too much time on this..
		GFile *file = g_file_new_for_path(path.c_str());
		GdkContentProvider *contentProvider;
		GdkFileList* fileList = gdk_file_list_new_from_array(&file, 1);
		contentProvider = gdk_content_provider_new_typed(GDK_TYPE_FILE_LIST, fileList);

		return Glib::wrap(contentProvider);
	}, false);
	source->signal_drag_end().connect([](const Glib::RefPtr<Gdk::Drag>& drag, bool delete_data) {
		// TODO: This does not report the correct action
		//
		// DO NOT IMPLEMENT A DELETE FUNCTION UNTIL THIS IS RESOLVED
		//

		auto selected = drag->get_selected_action();
		if ((selected & Gdk::DragAction::MOVE) == Gdk::DragAction::MOVE)
			std::printf("Move\n");
		else if ((selected & Gdk::DragAction::COPY) == Gdk::DragAction::COPY)
			std::printf("Copy\n");

		if (delete_data)
			std::printf("Delete data\n");
		else
			std::printf("Don't delete data\n");
	});
	add_controller(source);

	// Figure out what icon to use
	if (is_directory) {
		if (xdg_dirs[path] != "")
			file_icon = xdg_dirs[path];
		else
			file_icon = "default-folder";
		setup_drop_target();
	}
	else {
		size_t last_dot_pos = file_name.rfind('.');
		if (last_dot_pos != std::string::npos) {
			extension = file_name.substr(last_dot_pos + 1);
			std::transform(extension.begin(), extension.end(), extension.begin(),
				[](unsigned char c) { return std::tolower(c); });
			std::string extension_icon = icon_from_extension[extension];
			if (!extension_icon.empty())
				file_icon = icon_from_extension[extension];
			else
				file_icon = "application-blank";
		}
		else {
			// TODO: Re add magic file detection for fallback
			file_icon = "application-blank";
		}
	}

	// Load icons
	auto icon_theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
	auto icon_info = icon_theme->lookup_icon(file_icon, icon_size);
	auto file = icon_info->get_file();
	auto icon = Gdk::Texture::create_from_file(file);

	image.set(icon);
	source->set_icon(icon, icon_size / 2, icon_size / 2);
}

void file_entry::load_thumbnail() {
	// TODO: SVGS look terrible
	// TODO: Maybe consider animated gifs?
	if (icon_from_extension[extension].find("image") != std::string::npos) {
		pixbuf = Gdk::Pixbuf::create_from_file(path);
	}
	else if (icon_from_extension[extension].find("video") != std::string::npos && false) { // Temporarily disabled because it's buggy
		GstElement* pipeline;
		GstSample* sample = nullptr;
		GError* error = nullptr;

		gst_init(nullptr, nullptr);

		std::string pipeline_str = "uridecodebin uri=" +  Glib::filename_to_uri(path) + " ! videoconvert ! appsink name=sink caps=video/x-raw,format=RGB";

		pipeline = gst_parse_launch(pipeline_str.c_str(), &error);
		GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");

		g_object_set(sink, "max-buffers", 1, "drop", true, nullptr);
		GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
		if (ret == GST_STATE_CHANGE_FAILURE)
			return;

		g_signal_emit_by_name(sink, "pull-sample", &sample);
		if (!sample)
			return;

		GstMapInfo map;
		GstBuffer* buffer = gst_sample_get_buffer(sample);
		if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
			return;

		gst_element_send_event(pipeline, gst_event_new_eos());

		GstVideoInfo info;
		GstCaps* caps = gst_sample_get_caps(sample);
		if (gst_video_info_from_caps(&info, caps)) {
			GdkPixbuf* gdk_pixbuf = gdk_pixbuf_new_from_data(
				map.data, GDK_COLORSPACE_RGB, false, 8,
				info.width, info.height, info.stride[0],
				nullptr, nullptr
			);

			gst_buffer_unmap(buffer, &map);

			gst_sample_unref(sample);
			pixbuf = Glib::wrap(gdk_pixbuf);
		}
		gst_object_unref(pipeline);
	}
	else
		return;

	image.set(resize_thumbnail(pixbuf));
}

Glib::RefPtr<Gdk::Pixbuf> file_entry::resize_thumbnail(Glib::RefPtr<Gdk::Pixbuf> pixbuf) {
	int img_width = pixbuf->get_width();
	int img_height = pixbuf->get_height();
	double scale = std::min(static_cast<double>(icon_size) / img_width, static_cast<double>(icon_size) / img_height);
	int new_width = static_cast<int>(img_width * scale);
	int new_height = static_cast<int>(img_height * scale);
	return pixbuf->scale_simple(new_width, new_height, Gdk::InterpType::BILINEAR);
}

void file_entry::setup_drop_target() {
	auto target = Gtk::DropTarget::create(GDK_TYPE_FILE_LIST, Gdk::DragAction::MOVE);
	target->signal_drop().connect([&](const Glib::ValueBase& value, double, double) {
		Glib::Value<GSList*> gslist_value;
		gslist_value.init(value.gobj());
		auto files = Glib::SListHandler<Glib::RefPtr<Gio::File>>::slist_to_vector(gslist_value.get(), Glib::OwnershipType::OWNERSHIP_NONE);
		for (const auto& file : files) {
			Glib::RefPtr<Gio::File> destination = Gio::File::create_for_path(path + "/" + file->get_basename());

			// TODO: Handle conflicts
			if (destination->query_exists()) {
				std::fprintf(stderr, "File already exists, Not moving file.\n");
				continue;
			}

			// TODO: Check for permissions (Both source and dest)
			std::printf("Moving file: %s\n", file->get_path().c_str());
			std::printf("To: %s\n", path.c_str());
			file->move(destination);
		}

		return true;
	}, false);
	add_controller(target);
}
