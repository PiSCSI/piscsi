//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include <pwd.h>
#include "log.h"
#include "filepath.h"
#include "spdlog/spdlog.h"
#include "devices/file_support.h"
#include "command_util.h"
#include "command_context.h"
#include "image.h"
#include <string>
#include <array>
#include <filesystem>
#ifdef __linux
#include <sys/sendfile.h>
#endif

using namespace std;
using namespace spdlog;
using namespace rascsi_interface;
using namespace command_util;

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
	if (size_t filename_start = filename.rfind('/'); filename_start != string::npos) {
		string folder = filename.substr(0, filename_start);

		// Checking for existence first prevents an error if the top-level folder is a softlink
		if (struct stat st; stat(folder.c_str(), &st)) {
			std::error_code error;
			filesystem::create_directories(folder, error);
			if (error) {
				context.ReturnStatus(false, "Can't create image folder '" + folder + "': " + strerror(errno));
				return false;
			}
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

	// If a relative path is specified the path is assumed to be relative to the user's home directory
	if (folder[0] != '/') {
		folder = GetHomeDir() + "/" + f;
	}
	else {
		if (folder.find("/home/") != 0) {
			return "Default image folder must be located in '/home/'";
		}
	}

	struct stat info;
	stat(folder.c_str(), &info);
	if (!S_ISDIR(info.st_mode) || access(folder.c_str(), F_OK) == -1) {
		return "Folder '" + f + "' does not exist or is not accessible";
	}

	default_folder = folder;

	LOGINFO("Default image folder set to '%s'", default_folder.c_str())

	return "";
}

bool RascsiImage::IsValidSrcFilename(const string& filename) const
{
	// Source file must exist and must be a regular file or a symlink
	struct stat st;
	return !stat(filename.c_str(), &st) && (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode));
}

bool RascsiImage::IsValidDstFilename(const string& filename) const
{
	// Destination file must not yet exist
	struct stat st;
	return stat(filename.c_str(), &st);
}

bool RascsiImage::CreateImage(const CommandContext& context, const PbCommand& command) const
{
	string filename = GetParam(command, "file");
	if (filename.empty()) {
		return context.ReturnStatus(false, "Can't create image file: Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + filename + "'").c_str());
	}

	string full_filename = GetFullName(filename);
	if (!IsValidDstFilename(full_filename)) {
		return context.ReturnStatus(false, "Can't create image file: '" + full_filename + "': File already exists");
	}

	const string size = GetParam(command, "size");
	if (size.empty()) {
		return context.ReturnStatus(false, "Can't create image file '" + full_filename + "': Missing image size");
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

	string permission = GetParam(command, "read_only");
	// Since rascsi is running as root ensure that others can access the file
	int permissions = !strcasecmp(permission.c_str(), "true") ?
			S_IRUSR | S_IRGRP | S_IROTH : S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	int image_fd = open(full_filename.c_str(), O_CREAT|O_WRONLY, permissions);
	if (image_fd == -1) {
		return context.ReturnStatus(false, "Can't create image file '" + full_filename + "': " + string(strerror(errno)));
	}

#ifndef __linux
	close(image_fd);

	unlink(full_filename.c_str());

	return false;
#else
	if (fallocate(image_fd, 0, 0, len)) {
		close(image_fd);

		unlink(full_filename.c_str());

		return context.ReturnStatus(false, "Can't allocate space for image file '" + full_filename + "': " + string(strerror(errno)));
	}

	close(image_fd);

	LOGINFO("%s", string("Created " + string(permissions & S_IWUSR ? "": "read-only ") + "image file '" + full_filename +
			"' with a size of " + to_string(len) + " bytes").c_str())

	return context.ReturnStatus();
#endif
}

bool RascsiImage::DeleteImage(const CommandContext& context, const PbCommand& command) const
{
	string filename = GetParam(command, "file");
	if (filename.empty()) {
		return context.ReturnStatus(false, "Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + filename + "'").c_str());
	}

	string full_filename = GetFullName(filename);

	int id;
	int unit;
	Filepath filepath;
	filepath.SetPath(full_filename.c_str());
	if (FileSupport::GetIdsForReservedFile(filepath, id, unit)) {
		return context.ReturnStatus(false, "Can't delete image file '" + full_filename +
				"', it is currently being used by device ID " + to_string(id) + ", unit " + to_string(unit));
	}

	if (remove(full_filename.c_str())) {
		return context.ReturnStatus(false, "Can't delete image file '" + full_filename + "': " + string(strerror(errno)));
	}

	// Delete empty subfolders
	size_t last_slash = filename.rfind('/');
	while (last_slash != string::npos) {
		string folder = filename.substr(0, last_slash);
		string full_folder = GetFullName(folder);

		if (error_code error; !filesystem::is_empty(full_folder, error) || error) {
			break;
		}

		if (remove(full_folder.c_str())) {
			return context.ReturnStatus(false, "Can't delete empty image folder '" + full_folder + "'");
		}

		last_slash = folder.rfind('/');
	}

	LOGINFO("Deleted image file '%s'", full_filename.c_str())

	return context.ReturnStatus();
}

bool RascsiImage::RenameImage(const CommandContext& context, const PbCommand& command) const
{
	string from = GetParam(command, "from");
	if (from.empty()) {
		return context.ReturnStatus(false, "Can't rename/move image file: Missing source filename");
	}

	if (!CheckDepth(from)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + from + "'").c_str());
	}

	from = GetFullName(from);
	if (!IsValidSrcFilename(from)) {
		return context.ReturnStatus(false, "Can't rename/move image file: '" + from + "': Invalid name or type");
	}

	string to = GetParam(command, "to");
	if (to.empty()) {
		return context.ReturnStatus(false, "Can't rename/move image file '" + from + "': Missing destination filename");
	}

	if (!CheckDepth(to)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + to + "'").c_str());
	}

	to = GetFullName(to);
	if (!IsValidDstFilename(to)) {
		return context.ReturnStatus(false, "Can't rename/move image file '" + from + "' to '" + to + "': File already exists");
	}

	if (!CreateImageFolder(context, to)) {
		return false;
	}

	if (rename(from.c_str(), to.c_str())) {
		return context.ReturnStatus(false, "Can't rename/move image file '" + from + "' to '" + to + "': " + string(strerror(errno)));
	}

	LOGINFO("Renamed/Moved image file '%s' to '%s'", from.c_str(), to.c_str())

	return context.ReturnStatus();
}

bool RascsiImage::CopyImage(const CommandContext& context, const PbCommand& command) const
{
	string from = GetParam(command, "from");
	if (from.empty()) {
		return context.ReturnStatus(false, "Can't copy image file: Missing source filename");
	}

	if (!CheckDepth(from)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + from + "'").c_str());
	}

	from = GetFullName(from);
	if (!IsValidSrcFilename(from)) {
		return context.ReturnStatus(false, "Can't copy image file: '" + from + "': Invalid name or type");
	}

	string to = GetParam(command, "to");
	if (to.empty()) {
		return context.ReturnStatus(false, "Can't copy image file '" + from + "': Missing destination filename");
	}

	if (!CheckDepth(to)) {
		return context.ReturnStatus(false, ("Invalid folder hierarchy depth '" + to + "'").c_str());
	}

	to = GetFullName(to);
	if (!IsValidDstFilename(to)) {
		return context.ReturnStatus(false, "Can't copy image file '" + from + "' to '" + to + "': File already exists");
	}

	struct stat st;
    if (lstat(from.c_str(), &st)) {
    	return context.ReturnStatus(false, "Can't access source image file '" + from + "': " + string(strerror(errno)));
    }

	if (!CreateImageFolder(context, to)) {
		return false;
	}

    // Symbolic links need a special handling
	if ((st.st_mode & S_IFMT) == S_IFLNK) {
		if (symlink(filesystem::read_symlink(from).c_str(), to.c_str())) {
	    	return context.ReturnStatus(false, "Can't copy symlink '" + from + "': " + string(strerror(errno)));
		}

		LOGINFO("Copied symlink '%s' to '%s'", from.c_str(), to.c_str())

		return context.ReturnStatus();
	}

	int fd_src = open(from.c_str(), O_RDONLY, 0);
	if (fd_src == -1) {
		return context.ReturnStatus(false, "Can't open source image file '" + from + "': " + string(strerror(errno)));
	}

	string permission = GetParam(command, "read_only");
	// Since rascsi is running as root ensure that others can access the file
	int permissions = !strcasecmp(permission.c_str(), "true") ?
			S_IRUSR | S_IRGRP | S_IROTH : S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	int fd_dst = open(to.c_str(), O_WRONLY | O_CREAT, permissions);
	if (fd_dst == -1) {
		close(fd_src);

		return context.ReturnStatus(false, "Can't open destination image file '" + to + "': " + string(strerror(errno)));
	}

#ifndef __linux
    close(fd_dst);
    close(fd_src);

	unlink(to.c_str());

	return false;
#else
    if (sendfile(fd_dst, fd_src, nullptr, st.st_size) == -1) {
        close(fd_dst);
        close(fd_src);

		unlink(to.c_str());

        return context.ReturnStatus(false, "Can't copy image file '" + from + "' to '" + to + "': " + string(strerror(errno)));
	}

    close(fd_dst);
    close(fd_src);

	LOGINFO("Copied image file '%s' to '%s'", from.c_str(), to.c_str())

	return context.ReturnStatus();
#endif
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

	bool protect = command.operation() == PROTECT_IMAGE;

	if (int permissions = protect ? S_IRUSR | S_IRGRP | S_IROTH : S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
		chmod(filename.c_str(), permissions) == -1) {
		return context.ReturnStatus(false, "Can't " + string(protect ? "protect" : "unprotect") + " image file '" + filename + "': " +
				strerror(errno));
	}

	if (protect) {
		LOGINFO("Protected image file '%s'", filename.c_str())
	}
	else {
		LOGINFO("Unprotected image file '%s'", filename.c_str())
	}

	return context.ReturnStatus();
}

string RascsiImage::GetHomeDir() const
{
	int uid = getuid();
	if (const char *sudo_user = getenv("SUDO_UID"); sudo_user != nullptr) {
		uid = stoi(sudo_user);
	}

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
