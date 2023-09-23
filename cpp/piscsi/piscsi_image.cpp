//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "devices/disk.h"
#include "piscsi_image.h"
#include "shared/protobuf_util.h"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <pwd.h>
#include <fstream>
#include <array>

using namespace std;
using namespace filesystem;
using namespace piscsi_interface;
using namespace protobuf_util;

PiscsiImage::PiscsiImage()
{
	// ~/images is the default folder for device image files, for the root user it is /home/pi/images
	default_folder = GetHomeDir() + "/images";
}

bool PiscsiImage::CheckDepth(string_view filename) const
{
	return count(filename.begin(), filename.end(), '/') <= depth;
}

bool PiscsiImage::CreateImageFolder(const CommandContext& context, string_view filename) const
{
	if (const auto folder = path(filename).parent_path(); !folder.string().empty()) {
		// Checking for existence first prevents an error if the top-level folder is a softlink
		if (error_code error; exists(folder, error)) {
			return true;
		}

		try {
			create_directories(folder);

			return ChangeOwner(context, folder, false);
		}
		catch(const filesystem_error& e) {
			return context.ReturnErrorStatus("Can't create image folder '" + folder.string() + "': " + e.what());
		}
	}

	return true;
}

string PiscsiImage::SetDefaultFolder(string_view f)
{
	if (f.empty()) {
		return "Can't set default image folder: Missing folder name";
	}

	// If a relative path is specified, the path is assumed to be relative to the user's home directory
	path folder(f);
	if (folder.is_relative()) {
		folder = path(GetHomeDir() + "/" + folder.string());
	}

	if (path home_root = path(GetHomeDir()).parent_path(); !folder.string().starts_with(home_root.string())) {
		return "Default image folder must be located in '" + home_root.string() + "'";
	}

	// Resolve a potential symlink
	if (error_code error; is_symlink(folder, error)) {
		folder = read_symlink(folder);
	}

	if (error_code error; !is_directory(folder)) {
		return string("'") + folder.string() + "' is not a valid image folder";
	}

	default_folder = folder.string();

	spdlog::info("Default image folder set to '" + default_folder + "'");

	return "";
}

bool PiscsiImage::CreateImage(const CommandContext& context) const
{
	const string filename = GetParam(context.GetCommand(), "file");
	if (filename.empty()) {
		return context.ReturnErrorStatus("Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return context.ReturnErrorStatus(("Invalid folder hierarchy depth '" + filename + "'").c_str());
	}

	const string full_filename = GetFullName(filename);
	if (!IsValidDstFilename(full_filename)) {
		return context.ReturnErrorStatus("Can't create image file: '" + full_filename + "': File already exists");
	}

	const string size = GetParam(context.GetCommand(), "size");
	if (size.empty()) {
		return context.ReturnErrorStatus("Can't create image file '" + full_filename + "': Missing file size");
	}

	off_t len;
	try {
		len = stoull(size);
	}
	catch(const invalid_argument&) {
		return context.ReturnErrorStatus("Can't create image file '" + full_filename + "': Invalid file size " + size);
	}
	catch(const out_of_range&) {
		return context.ReturnErrorStatus("Can't create image file '" + full_filename + "': Invalid file size " + size);
	}
	if (len < 512 || (len & 0x1ff)) {
		return context.ReturnErrorStatus("Invalid image file size " + to_string(len) + " (not a multiple of 512)");
	}

	if (!CreateImageFolder(context, full_filename)) {
		return false;
	}

	const bool read_only = GetParam(context.GetCommand(), "read_only") == "true";

	error_code error;
	path file(full_filename);
	try {
		ofstream s(file);
		s.close();

		if (!ChangeOwner(context, file, read_only)) {
			return false;
		}

		resize_file(file, len);
	}
	catch(const filesystem_error& e) {
		remove(file, error);

		return context.ReturnErrorStatus("Can't create image file '" + full_filename + "': " + e.what());
	}

	spdlog::info("Created " + string(read_only ? "read-only " : "") + "image file '" + full_filename +
			"' with a size of " + to_string(len) + " bytes");

	return context.ReturnSuccessStatus();
}

bool PiscsiImage::DeleteImage(const CommandContext& context) const
{
	const string filename = GetParam(context.GetCommand(), "file");
	if (filename.empty()) {
		return context.ReturnErrorStatus("Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return context.ReturnErrorStatus("Invalid folder hierarchy depth '" + filename + "'");
	}

	const auto full_filename = path(GetFullName(filename));
	if (!exists(full_filename)) {
		return context.ReturnErrorStatus("Image file '" + full_filename.string() + "' does not exist");
	}

	const auto [id, lun] = StorageDevice::GetIdsForReservedFile(full_filename);
	if (id != -1 || lun != -1) {
		return context.ReturnErrorStatus("Can't delete image file '" + full_filename.string() +
				"', it is currently being used by device ID " + to_string(id) + ", LUN " + to_string(lun));
	}

	if (error_code error; !remove(full_filename, error)) {
		return context.ReturnErrorStatus("Can't delete image file '" + full_filename.string() + "'");
	}

	// Delete empty subfolders
	size_t last_slash = filename.rfind('/');
	while (last_slash != string::npos) {
		const string folder = filename.substr(0, last_slash);
		const auto full_folder = path(GetFullName(folder));

		if (error_code error; !filesystem::is_empty(full_folder, error) || error) {
			break;
		}

		if (error_code error; !remove(full_folder)) {
			return context.ReturnErrorStatus("Can't delete empty image folder '" + full_folder.string() +  "'");
		}

		last_slash = folder.rfind('/');
	}

	spdlog::info("Deleted image file '" + full_filename.string() + "'");

	return context.ReturnSuccessStatus();
}

bool PiscsiImage::RenameImage(const CommandContext& context) const
{
	string from;
	string to;
	if (!ValidateParams(context, "rename/move", from, to)) {
		return false;
	}

	const auto [id, lun] = StorageDevice::GetIdsForReservedFile(from);
	if (id != -1 || lun != -1) {
		return context.ReturnErrorStatus("Can't rename/move image file '" + from +
				"', it is currently being used by device ID " + to_string(id) + ", LUN " + to_string(lun));
	}

	if (!CreateImageFolder(context, to)) {
		return false;
	}

	try {
		rename(path(from), path(to));
	}
	catch(const filesystem_error& e) {
		return context.ReturnErrorStatus("Can't rename/move image file '" + from + "' to '" + to + "': " + e.what());
	}

	spdlog::info("Renamed/Moved image file '" + from + "' to '" + to + "'");

	return context.ReturnSuccessStatus();
}

bool PiscsiImage::CopyImage(const CommandContext& context) const
{
	string from;
	string to;
	if (!ValidateParams(context, "copy", from, to)) {
		return false;
	}

	if (access(from.c_str(), R_OK)) {
    	return context.ReturnErrorStatus("Can't read source image file '" + from + "'");
    }

	const auto [id, lun] = StorageDevice::GetIdsForReservedFile(from);
	if (id != -1 || lun != -1) {
		return context.ReturnErrorStatus("Can't copy image file '" + from +
				"', it is currently being used by device ID " + to_string(id) + ", LUN " + to_string(lun));
	}

	if (!CreateImageFolder(context, to)) {
		return false;
	}

	path f(from);
	path t(to);

    // Symbolic links need a special handling
	if (error_code error; is_symlink(f, error)) {
		try {
			copy_symlink(f, t);
		}
		catch(const filesystem_error& e) {
	    	return context.ReturnErrorStatus("Can't copy image file symlink '" + from + "': " + e.what());
		}

		spdlog::info("Copied image file symlink '" + from + "' to '"  + to + "'");

		return context.ReturnSuccessStatus();
	}

	try {
		copy_file(f, t);

		permissions(t, GetParam(context.GetCommand(), "read_only") == "true" ?
				perms::owner_read | perms::group_read | perms::others_read :
				perms::owner_read | perms::group_read | perms::others_read |
				perms::owner_write | perms::group_write);
	}
	catch(const filesystem_error& e) {
        return context.ReturnErrorStatus("Can't copy image file '" + from + "' to '" + to + "': " + e.what());
	}

	spdlog::info("Copied image file '" + from + "' to '" + to + "'");

	return context.ReturnSuccessStatus();
}

bool PiscsiImage::SetImagePermissions(const CommandContext& context) const
{
	const string filename = GetParam(context.GetCommand(), "file");
	if (filename.empty()) {
		return context.ReturnErrorStatus("Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return context.ReturnErrorStatus("Invalid folder hierarchy depth '" + filename + "'");
	}

	const string full_filename = GetFullName(filename);
	if (!IsValidSrcFilename(full_filename)) {
		return context.ReturnErrorStatus("Can't modify image file '" + full_filename + "': Invalid name or type");
	}

	const bool protect = context.GetCommand().operation() == PROTECT_IMAGE;

	try {
		permissions(path(full_filename), protect ?
				perms::owner_read | perms::group_read | perms::others_read :
				perms::owner_read | perms::group_read | perms::others_read |
				perms::owner_write | perms::group_write);
	}
	catch(const filesystem_error& e) {
		return context.ReturnErrorStatus("Can't " + string(protect ? "protect" : "unprotect") + " image file '" +
				full_filename + "': " + e.what());
	}

	spdlog::info((protect ? "Protected" : "Unprotected") + string(" image file '") + full_filename + "'");

	return context.ReturnSuccessStatus();
}

bool PiscsiImage::ValidateParams(const CommandContext& context, const string& operation, string& from, string& to) const
{
	from = GetParam(context.GetCommand(), "from");
	if (from.empty()) {
		return context.ReturnErrorStatus("Can't " + operation + " image file: Missing source filename");
	}

	if (!CheckDepth(from)) {
		return context.ReturnErrorStatus("Invalid folder hierarchy depth '" + from + "'");
	}

	to = GetParam(context.GetCommand(), "to");
	if (to.empty()) {
		return context.ReturnErrorStatus("Can't " + operation + " image file '" + from + "': Missing destination filename");
	}

	if (!CheckDepth(to)) {
		return context.ReturnErrorStatus("Invalid folder hierarchy depth '" + to + "'");
	}

	from = GetFullName(from);
	if (!IsValidSrcFilename(from)) {
		return context.ReturnErrorStatus("Can't " + operation + " image file: '" + from + "': Invalid name or type");
	}

	to = GetFullName(to);
	if (!IsValidDstFilename(to)) {
		return context.ReturnErrorStatus("Can't " + operation + " image file '" + from + "' to '" + to + "': File already exists");
	}

	return true;
}

bool PiscsiImage::IsValidSrcFilename(string_view filename)
{
	// Source file must exist and must be a regular file or a symlink
	path file(filename);

	error_code error;
	return is_regular_file(file, error) || is_symlink(file, error);
}

bool PiscsiImage::IsValidDstFilename(string_view filename)
{
	// Destination file must not yet exist
	try {
		return !exists(path(filename));
	}
	catch(const filesystem_error&) {
		return false;
	}
}

bool PiscsiImage::ChangeOwner(const CommandContext& context, const path& filename, bool read_only)
{
	const auto [uid, gid] = GetUidAndGid();
	if (chown(filename.c_str(), uid, gid)) {
		// Remember the current error before the next filesystem operation
		const int e = errno;

		error_code error;
		remove(filename, error);

		return context.ReturnErrorStatus("Can't change ownership of '" + filename.string() + "': " + strerror(e));
	}

	permissions(filename, read_only ?
			perms::owner_read | perms::group_read | perms::others_read :
			perms::owner_read | perms::group_read | perms::others_read |
			perms::owner_write | perms::group_write);

	return true;
}

string PiscsiImage::GetHomeDir()
{
	const auto [uid, gid] = GetUidAndGid();

	passwd pwd = {};
	passwd *p_pwd;
	array<char, 256> pwbuf;

	if (uid && !getpwuid_r(uid, &pwd, pwbuf.data(), pwbuf.size(), &p_pwd)) {
		return pwd.pw_dir;
	}
	else {
		return "/home/pi";
	}
}

pair<int, int> PiscsiImage::GetUidAndGid()
{
	int uid = getuid();
	if (const char *sudo_user = getenv("SUDO_UID"); sudo_user != nullptr) {
		uid = stoi(sudo_user);
	}

	passwd pwd = {};
	passwd *p_pwd;
	array<char, 256> pwbuf;

	int gid = -1;
	if (!getpwuid_r(uid, &pwd, pwbuf.data(), pwbuf.size(), &p_pwd)) {
		gid = pwd.pw_gid;
	}

	return { uid, gid };
}
