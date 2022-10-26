//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "devices/disk.h"
#include "protobuf_util.h"
#include "command_context.h"
#include "rascsi_image.h"
#include <unistd.h>
#include <pwd.h>
#include <fstream>
#include <string>
#include <array>
#include <filesystem>

using namespace std;
using namespace filesystem;
using namespace rascsi_interface;
using namespace protobuf_util;

RascsiImage::RascsiImage()
{
	// ~/images is the default folder for device image files, for the root user it is /home/pi/images
	default_folder = GetHomeDir() + "/images";
}

bool RascsiImage::CheckDepth(string_view filename) const
{
	return count(filename.begin(), filename.end(), '/') <= depth;
}

bool RascsiImage::CreateImageFolder(const CommandContext& context, const string& filename) const
{
	if (const size_t filename_start = filename.rfind('/'); filename_start != string::npos) {
		const auto folder = path(filename.substr(0, filename_start));

		// Checking for existence first prevents an error if the top-level folder is a softlink
		if (error_code error; exists(folder, error)) {
			return true;
		}

		try {
			create_directories(folder);

			return ChangeOwner(context, folder, false);
		}
		catch(const filesystem_error& e) {
			return context.ReturnStatus(false, "Can't create image folder '" + string(folder) + "': " + e.what());
		}
	}

	return true;
}

string RascsiImage::SetDefaultFolder(const string& f)
{
	if (f.empty()) {
		return "Can't set default image folder: Missing folder name";
	}

	string folder = f;

	// If a relative path is specified, the path is assumed to be relative to the user's home directory
	if (folder[0] != '/') {
		folder = GetHomeDir() + "/" + folder;
	}
	else {
		if (folder.find("/home/") != 0) {
			return "Default image folder must be located in '/home/'";
		}
	}

	// Resolve a potential symlink
	auto p = path(folder);
	if (error_code error; is_symlink(p, error)) {
		p = read_symlink(p);
	}

	if (error_code error; !is_directory(p, error)) {
		return "'" + string(p) + "' is not a valid folder";
	}

	default_folder = string(p);

	LOGINFO("Default image folder set to '%s'", default_folder.c_str())

	return "";
}

bool RascsiImage::CreateImage(const CommandContext& context, const PbCommand& command) const
{
	const string filename = GetParam(command, "file");
	if (filename.empty()) {
		return context.ReturnStatus(false, "Can't create image file: Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + filename + "'").c_str());
	}

	const string full_filename = GetFullName(filename);
	if (!IsValidDstFilename(full_filename)) {
		return context.ReturnStatus(false, "Can't create image file: '" + full_filename + "': File already exists");
	}

	const string size = GetParam(command, "size");
	if (size.empty()) {
		return context.ReturnStatus(false, "Can't create image file '" + full_filename + "': Missing file size");
	}

	off_t len;
	try {
		len = stoull(size);
	}
	catch(const invalid_argument&) {
		return context.ReturnStatus(false, "Can't create image file '" + full_filename + "': Invalid file size " + size);
	}
	catch(const out_of_range&) {
		return context.ReturnStatus(false, "Can't create image file '" + full_filename + "': Invalid file size " + size);
	}
	if (len < 512 || (len & 0x1ff)) {
		return context.ReturnStatus(false, "Invalid image file size " + to_string(len) + " (not a multiple of 512)");
	}

	if (!CreateImageFolder(context, full_filename)) {
		return false;
	}

	const bool read_only = GetParam(command, "read_only") == "true";

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

		return context.ReturnStatus(false, "Can't create image file '" + full_filename + "': " + e.what());
	}

	LOGINFO("%s", string("Created " + string(read_only ? "read-only " : "") + "image file '" + full_filename +
			"' with a size of " + to_string(len) + " bytes").c_str())

	return context.ReturnStatus();
}

bool RascsiImage::DeleteImage(const CommandContext& context, const PbCommand& command) const
{
	const string filename = GetParam(command, "file");
	if (filename.empty()) {
		return context.ReturnStatus(false, "Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + filename + "'").c_str());
	}

	const string full_filename = path(GetFullName(filename));

	if (!exists(path(full_filename))) {
		return context.ReturnStatus(false, "Image file '" + full_filename + "' does not exist");
	}

	const auto [id, lun] = StorageDevice::GetIdsForReservedFile(full_filename);
	if (id != -1 && lun != -1) {
		return context.ReturnStatus(false, "Can't delete image file '" + full_filename +
				"', it is currently being used by device ID " + to_string(id) + ", LUN " + to_string(lun));
	}

	if (error_code error; !remove(path(full_filename), error)) {
		return context.ReturnStatus(false, "Can't delete image file '" + full_filename + "'");
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
			return context.ReturnStatus(false, "Can't delete empty image folder '" + string(full_folder) +  "'");
		}

		last_slash = folder.rfind('/');
	}

	LOGINFO("Deleted image file '%s'", full_filename.c_str())

	return context.ReturnStatus();
}

bool RascsiImage::RenameImage(const CommandContext& context, const PbCommand& command) const
{
	string from;
	string to;
	if (!ValidateParams(context, command, "rename/move", from, to)) {
		return false;
	}

	if (!CreateImageFolder(context, to)) {
		return false;
	}

	try {
		rename(path(from), path(to));
	}
	catch(const filesystem_error& e) {
		return context.ReturnStatus(false, "Can't rename/move image file '" + from + "' to '" + to + "': " + e.what());
	}

	LOGINFO("Renamed/Moved image file '%s' to '%s'", from.c_str(), to.c_str())

	return context.ReturnStatus();
}

bool RascsiImage::CopyImage(const CommandContext& context, const PbCommand& command) const
{
	string from;
	string to;
	if (!ValidateParams(context, command, "copy", from, to)) {
		return false;
	}

	if (access(from.c_str(), R_OK)) {
    	return context.ReturnStatus(false, "Can't read source image file '" + from + "'");
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
	    	return context.ReturnStatus(false, "Can't copy image file symlink '" + from + "': " + e.what());
		}

		LOGINFO("Copied image file symlink '%s' to '%s'", from.c_str(), to.c_str())

		return context.ReturnStatus();
	}

	try {
		copy_file(f, t);

		permissions(t, GetParam(command, "read_only") == "true" ?
				perms::owner_read | perms::group_read | perms::others_read :
				perms::owner_read | perms::group_read | perms::others_read |
				perms::owner_write | perms::group_write);
	}
	catch(const filesystem_error& e) {
        return context.ReturnStatus(false, "Can't copy image file '" + from + "' to '" + to + "': " + e.what());
	}

	LOGINFO("Copied image file '%s' to '%s'", from.c_str(), to.c_str())

	return context.ReturnStatus();
}

bool RascsiImage::SetImagePermissions(const CommandContext& context, const PbCommand& command) const
{
	string filename = GetParam(command, "file");
	if (filename.empty()) {
		return context.ReturnStatus(false, "Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + filename + "'").c_str());
	}

	filename = GetFullName(filename);
	if (!IsValidSrcFilename(filename)) {
		return context.ReturnStatus(false, "Can't modify image file '" + filename + "': Invalid name or type");
	}

	const bool protect = command.operation() == PROTECT_IMAGE;

	try {
		permissions(path(filename), protect ?
				perms::owner_read | perms::group_read | perms::others_read :
				perms::owner_read | perms::group_read | perms::others_read |
				perms::owner_write | perms::group_write);
	}
	catch(const filesystem_error& e) {
		return context.ReturnStatus(false, "Can't " + string(protect ? "protect" : "unprotect") + " image file '" +
				filename + "': " + e.what());
	}

	if (protect) {
		LOGINFO("Protected image file '%s'", filename.c_str())
	}
	else {
		LOGINFO("Unprotected image file '%s'", filename.c_str())
	}

	return context.ReturnStatus();
}

bool RascsiImage::ValidateParams(const CommandContext& context, const PbCommand& command, const string& operation,
		string& from, string& to) const
{
	from = GetParam(command, "from");
	if (from.empty()) {
		return context.ReturnStatus(false, "Can't " + operation + " image file: Missing source filename");
	}

	if (!CheckDepth(from)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + from + "'").c_str());
	}

	from = GetFullName(from);
	if (!IsValidSrcFilename(from)) {
		return context.ReturnStatus(false, "Can't " + operation + " image file: '" + from + "': Invalid name or type");
	}

	to = GetParam(command, "to");
	if (to.empty()) {
		return context.ReturnStatus(false, "Can't " + operation + " image file '" + from + "': Missing destination filename");
	}

	if (!CheckDepth(to)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + to + "'").c_str());
	}

	to = GetFullName(to);
	if (!IsValidDstFilename(to)) {
		return context.ReturnStatus(false, "Can't " + operation + " image file '" + from + "' to '" + to + "': File already exists");
	}

	return true;
}

bool RascsiImage::IsValidSrcFilename(const string& filename)
{
	// Source file must exist and must be a regular file or a symlink
	path file(filename);
	return is_regular_file(file) || is_symlink(file);
}

bool RascsiImage::IsValidDstFilename(const string& filename)
{
	// Destination file must not yet exist
	try {
		return !exists(path(filename));
	}
	catch(const filesystem_error&) {
		return true;
	}
}

bool RascsiImage::ChangeOwner(const CommandContext& context, const path& filename, bool read_only)
{
	const auto [uid, gid] = GetUidAndGid();
	if (chown(filename.c_str(), uid, gid)) {
		// Remember the current error before the next filesystem operation
		const int e = errno;

		error_code error;
		remove(filename, error);

		return context.ReturnStatus(false, "Can't change ownership of '" + string(filename) + "': " + strerror(e));
	}

	permissions(filename, read_only ?
			perms::owner_read | perms::group_read | perms::others_read :
			perms::owner_read | perms::group_read | perms::others_read |
			perms::owner_write | perms::group_write);

	return true;
}

string RascsiImage::GetHomeDir()
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

pair<int, int> RascsiImage::GetUidAndGid()
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

	return make_pair(uid, gid);
}
