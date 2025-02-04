#include "file.hpp"
#include "icons.hpp"
#include "xdg_dirs.hpp"

#include "window.hpp"

#include <gtkmm/droptarget.h>
#include <gtkmm/stack.h>
#include <gtkmm/label.h>
#include <gtkmm/text.h>
#include <giomm/file.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/flowbox.h>
#include <glibmm/vectorutils.h>
#include <glibmm/convert.h>
#include <gstreamer-1.0/gst/gst.h>
#include <gst/video/video-info.h>

file_entry::file_entry(frog* win, const std::filesystem::directory_entry &entry) : Gtk::Box(Gtk::Orientation::VERTICAL), win(win), file_size(0), content_count(0), label("Loading.."), entry(entry) {
	get_style_context()->add_class("file_entry");

	append(image);
	image.set_pixel_size(win->file_icon_size);
	image.set_from_icon_name("content-loading-symbolic");

	append(label);
	label.set_halign(Gtk::Align::CENTER);
	auto stack = dynamic_cast<Gtk::Stack*>(label.get_children()[0]);
	auto ulabel = dynamic_cast<Gtk::Label*>(stack->get_children()[0]);
	auto text = dynamic_cast<Gtk::Text*>(stack->get_children()[1]);
	stack->set_hhomogeneous(false);
	ulabel->set_lines(2);
	ulabel->set_justify(Gtk::Justification::CENTER);
	ulabel->set_ellipsize(Pango::EllipsizeMode::END);
	ulabel->set_max_width_chars(win->file_label_limit);
	ulabel->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
	text->set_propagate_text_width(true);
	text->set_max_width_chars(win->file_label_limit);

	// Handle renaming
	label.property_editing().signal_changed().connect([&, entry]() {
		if (label.get_text() != file_name.c_str()) {
			std::string cur_path = entry.path().parent_path();
			std::string new_name = label.get_text();
			std::filesystem::rename(path, cur_path + "/" + new_name);
		}
	});

	is_directory = entry.is_directory();
	file_icon = entry.is_directory() ? "folder" : "application-x-generic";
	image.set_from_icon_name(file_icon);

	// Set up drag and drop
	source = Gtk::DragSource::create();
	source->set_actions(Gdk::DragAction::MOVE);

	source->signal_prepare().connect([&](const double &x, const double &y) {
		auto flowbox = dynamic_cast<Gtk::FlowBox*>(get_parent()->get_parent());
		auto selected_entries = flowbox->get_selected_children();
		std::vector<GFile*> files;

		// TODO: Consider checking if the user has permission to move the files first?
		for (const auto& selected_entry : selected_entries) {
			auto slected_file_entry = dynamic_cast<file_entry*>(selected_entry->get_child());
			GFile* file = g_file_new_for_path(slected_file_entry->path.c_str());
			files.push_back(file);
		}
		GdkFileList* file_list = gdk_file_list_new_from_array(files.data(), files.size());

		// Cleanup
		for (GFile* file : files) {
			g_object_unref(file);
		}

		GdkContentProvider* contentProvider;
		contentProvider = gdk_content_provider_new_typed(GDK_TYPE_FILE_LIST, file_list);

		return Glib::wrap(contentProvider);
	}, false);
	add_controller(source);

	// TODO: This is not supposed to run here..
	load_data();
}

file_entry::~file_entry() {
	// TODO: Cleanup is still not perfect
	pixbuf.reset();
	image.clear();
	pixbuf = nullptr;
}

void file_entry::load_data() {
	path = entry.path();
	file_name = entry.path().filename().string();

	// TODO: Add option to show/hide hidden files
	// TODO: Add option to enable/disable shadowing of hidden files
	if (file_name[0] == '.')
		image.set_opacity(0.75);

	label.set_text(file_name);

	// Figure out what icon to use
	if (is_directory) {
		if (xdg_dirs[path] != "")
			file_icon = xdg_dirs[path];
		else
			file_icon = "default-folder";
		setup_drop_target();
	}
	else {
		const size_t& last_dot_pos = file_name.rfind('.');
		if (last_dot_pos != std::string::npos) {
			extension = file_name.substr(last_dot_pos + 1);
			std::transform(extension.begin(), extension.end(), extension.begin(),
				[](unsigned char c) { return std::tolower(c); });
			const std::string& extension_icon = icon_from_extension[extension];
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

	// TODO: This takes far too long to compute
	// Also uses a lot of ram per instance

	// Load icons
	auto icon_info = win->icon_theme->lookup_icon(file_icon, win->file_icon_size);
	auto file = icon_info->get_file();
	auto icon = Gdk::Texture::create_from_file(file);

	image.set(icon);
	source->set_icon(icon, win->file_icon_size / 2, win->file_icon_size / 2);
	icon.reset();
}

void file_entry::load_thumbnail() {
	// TODO: SVGS look terrible
	// TODO: Maybe consider animated gifs?
	if (icon_from_extension[extension].find("image") != std::string::npos) {
		try {
			pixbuf = Gdk::Pixbuf::create_from_file(path);
		}
		catch (const Gdk::PixbufError& e) {
			std::fprintf(stderr, "Unable to generate a thumbnail for file: %s\n", path.c_str());
			pixbuf.reset();
			pixbuf = nullptr;
			return;
		}
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
	pixbuf.reset();
}

void file_entry::load_metadata() {
	// TODO: Check for permission before loading metadata

	// Filesize
	if (is_directory) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied)) {
			if (std::filesystem::is_regular_file(entry)) {
				file_size += std::filesystem::file_size(entry);
			}
		}
	}
	else {
		file_size = entry.file_size();
	}

	// TODO: Maybe it would be nice to get the content type instead of general content? (Files/Dirs)
	// Content
	if (is_directory) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied)) {
			(void)entry;
			content_count++;
		}
	}
	// TODO: Add more metadata stuff (Image size, Video length, Ect..)
}

Glib::RefPtr<Gdk::Pixbuf> file_entry::resize_thumbnail(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf) {
	const int img_width = pixbuf->get_width();
	const int img_height = pixbuf->get_height();
	const double scale = std::min((double)win->file_icon_size / img_width, (double)win->file_icon_size / img_height);
	const int new_width = (int)(img_width * scale);
	const int new_height = (int)(img_height * scale);
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

			// TODO: Check if the user has permission to move files here
			std::printf("Moving file: %s\n", file->get_path().c_str());
			std::printf("To: %s\n", path.c_str());
			file->move(destination);
		}

		return true;
	}, false);
	add_controller(target);
}
