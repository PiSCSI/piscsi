//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include <sys/sendfile.h>
#include "os.h"
#include "log.h"
#include "filepath.h"
#include "spdlog/spdlog.h"
#include "devices/file_support.h"
#include "protobuf_util.h"
#include "rascsi_image.h"
#include <string>
#include <filesystem>

using namespace std;
using namespace spdlog;
using namespace rascsi_interface;
using namespace protobuf_util;

#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

RascsiImage::RascsiImage()
{
	// ~/images is the default folder for device image files, for the root user it is /home/pi/images
	int uid = getuid();
	const char *sudo_user = getenv("SUDO_UID");
	if (sudo_user) {
		uid = stoi(sudo_user);
	}

	const passwd *passwd = getpwuid(uid);
	if (uid && passwd) {
		default_image_folder = passwd->pw_dir;
		default_image_folder += "/images";
	}
	else {
		default_image_folder = "/home/pi/images";
	}

	depth = 1;
}

bool RascsiImage::CheckDepth(const string& filename)
{
	return count(filename.begin(), filename.end(), '/') <= depth;
}

bool RascsiImage::CreateImageFolder(const CommandContext& context, const string& filename)
{
	size_t filename_start = filename.rfind('/');
	if (filename_start != string::npos) {
		string folder = filename.substr(0, filename_start);

		// Checking for existence first prevents an error if the top-level folder is a softlink
		struct stat st;
		if (stat(folder.c_str(), &st)) {
			std::error_code error;
			filesystem::create_directories(folder, error);
			if (error) {
				ReturnStatus(context, false, "Can't create image folder '" + folder + "': " + strerror(errno));
				return false;
			}
		}
	}

	return true;
}

string RascsiImage::SetDefaultImageFolder(const string& f)
{
	if (f.empty()) {
		return "Can't set default image folder: Missing folder name";
	}

	string folder = f;

	// If a relative path is specified the path is assumed to be relative to the user's home directory
	if (folder[0] != '/') {
		int uid = getuid();
		const char *sudo_user = getenv("SUDO_UID");
		if (sudo_user) {
			uid = stoi(sudo_user);
		}

		const passwd *passwd = getpwuid(uid);
		if (passwd) {
			folder = passwd->pw_dir;
			folder += "/";
			folder += f;
		}
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

	default_image_folder = folder;

	LOGINFO("Default image folder set to '%s'", default_image_folder.c_str());

	return "";
}

bool RascsiImage::IsValidSrcFilename(const string& filename)
{
	// Source file must exist and must be a regular file or a symlink
	struct stat st;
	return !stat(filename.c_str(), &st) && (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode));
}

bool RascsiImage::IsValidDstFilename(const string& filename)
{
	// Destination file must not yet exist
	struct stat st;
	return stat(filename.c_str(), &st);
}

bool RascsiImage::CreateImage(const CommandContext& context, const PbCommand& command)
{
	string filename = GetParam(command, "file");
	if (filename.empty()) {
		return ReturnStatus(context, false, "Can't create image file: Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return ReturnStatus(context, false, ("Invalid folder hierarchy depth '" + filename + "'").c_str());
	}

	string full_filename = default_image_folder + "/" + filename;
	if (!IsValidDstFilename(full_filename)) {
		return ReturnStatus(context, false, "Can't create image file: '" + full_filename + "': File already exists");
	}

	const string size = GetParam(command, "size");
	if (size.empty()) {
		return ReturnStatus(context, false, "Can't create image file '" + full_filename + "': Missing image size");
	}

	off_t len;
	try {
		len = stoull(size);
	}
	catch(const invalid_argument& e) {
		return ReturnStatus(context, false, "Can't create image file '" + full_filename + "': Invalid file size " + size);
	}
	catch(const out_of_range& e) {
		return ReturnStatus(context, false, "Can't create image file '" + full_filename + "': Invalid file size " + size);
	}
	if (len < 512 || (len & 0x1ff)) {
		return ReturnStatus(context, false, "Invalid image file size " + to_string(len) + " (not a multiple of 512)");
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
		return ReturnStatus(context, false, "Can't create image file '" + full_filename + "': " + string(strerror(errno)));
	}

	if (fallocate(image_fd, 0, 0, len)) {
		close(image_fd);

		unlink(full_filename.c_str());

		return ReturnStatus(context, false, "Can't allocate space for image file '" + full_filename + "': " + string(strerror(errno)));
	}

	close(image_fd);

	LOGINFO("%s", string("Created " + string(permissions & S_IWUSR ? "": "read-only ") + "image file '" + full_filename +
			"' with a size of " + to_string(len) + " bytes").c_str());

	return ReturnStatus(context);
}

bool RascsiImage::DeleteImage(const CommandContext& context, const PbCommand& command)
{
	string filename = GetParam(command, "file");
	if (filename.empty()) {
		return ReturnStatus(context, false, "Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return ReturnStatus(context, false, ("Invalid folder hierarchy depth '" + filename + "'").c_str());
	}

	string full_filename = default_image_folder + "/" + filename;

	int id;
	int unit;
	Filepath filepath;
	filepath.SetPath(full_filename.c_str());
	if (FileSupport::GetIdsForReservedFile(filepath, id, unit)) {
		return ReturnStatus(context, false, "Can't delete image file '" + full_filename +
				"', it is currently being used by device ID " + to_string(id) + ", unit " + to_string(unit));
	}

	if (remove(full_filename.c_str())) {
		return ReturnStatus(context, false, "Can't delete image file '" + full_filename + "': " + string(strerror(errno)));
	}

	// Delete empty subfolders
	size_t last_slash = filename.rfind('/');
	while (last_slash != string::npos) {
		string folder = filename.substr(0, last_slash);
		string full_folder = default_image_folder + "/" + folder;

		std::error_code error;
		if (!filesystem::is_empty(full_folder, error) || error) {
			break;
		}

		if (remove(full_folder.c_str())) {
			return ReturnStatus(context, false, "Can't delete empty image folder '" + full_folder + "'");
		}

		last_slash = folder.rfind('/');
	}

	LOGINFO("Deleted image file '%s'", full_filename.c_str());

	return ReturnStatus(context);
}

bool RascsiImage::RenameImage(const CommandContext& context, const PbCommand& command)
{
	string from = GetParam(command, "from");
	if (from.empty()) {
		return ReturnStatus(context, false, "Can't rename/move image file: Missing source filename");
	}

	if (!CheckDepth(from)) {
		return ReturnStatus(context, false, ("Invalid folder hierarchy depth '" + from + "'").c_str());
	}

	from = default_image_folder + "/" + from;
	if (!IsValidSrcFilename(from)) {
		return ReturnStatus(context, false, "Can't rename/move image file: '" + from + "': Invalid name or type");
	}

	string to = GetParam(command, "to");
	if (to.empty()) {
		return ReturnStatus(context, false, "Can't rename/move image file '" + from + "': Missing destination filename");
	}

	if (!CheckDepth(to)) {
		return ReturnStatus(context, false, ("Invalid folder hierarchy depth '" + to + "'").c_str());
	}

	to = default_image_folder + "/" + to;
	if (!IsValidDstFilename(to)) {
		return ReturnStatus(context, false, "Can't rename/move image file '" + from + "' to '" + to + "': File already exists");
	}

	if (!CreateImageFolder(context, to)) {
		return false;
	}

	if (rename(from.c_str(), to.c_str())) {
		return ReturnStatus(context, false, "Can't rename/move image file '" + from + "' to '" + to + "': " + string(strerror(errno)));
	}

	LOGINFO("Renamed/Moved image file '%s' to '%s'", from.c_str(), to.c_str());

	return ReturnStatus(context);
}

bool RascsiImage::CopyImage(const CommandContext& context, const PbCommand& command)
{
	string from = GetParam(command, "from");
	if (from.empty()) {
		return ReturnStatus(context, false, "Can't copy image file: Missing source filename");
	}

	if (!CheckDepth(from)) {
		return ReturnStatus(context, false, ("Invalid folder hierarchy depth '" + from + "'").c_str());
	}

	from = default_image_folder + "/" + from;
	if (!IsValidSrcFilename(from)) {
		return ReturnStatus(context, false, "Can't copy image file: '" + from + "': Invalid name or type");
	}

	string to = GetParam(command, "to");
	if (to.empty()) {
		return ReturnStatus(context, false, "Can't copy image file '" + from + "': Missing destination filename");
	}

	if (!CheckDepth(to)) {
		return ReturnStatus(context, false, ("Invalid folder hierarchy depth '" + to + "'").c_str());
	}

	to = default_image_folder + "/" + to;
	if (!IsValidDstFilename(to)) {
		return ReturnStatus(context, false, "Can't copy image file '" + from + "' to '" + to + "': File already exists");
	}

	struct stat st;
    if (lstat(from.c_str(), &st)) {
    	return ReturnStatus(context, false, "Can't access source image file '" + from + "': " + string(strerror(errno)));
    }

	if (!CreateImageFolder(context, to)) {
		return false;
	}

    // Symbolic links need a special handling
	if ((st.st_mode & S_IFMT) == S_IFLNK) {
		if (symlink(filesystem::read_symlink(from).c_str(), to.c_str())) {
	    	return ReturnStatus(context, false, "Can't copy symlink '" + from + "': " + string(strerror(errno)));
		}

		LOGINFO("Copied symlink '%s' to '%s'", from.c_str(), to.c_str());

		return ReturnStatus(context);
	}

	int fd_src = open(from.c_str(), O_RDONLY, 0);
	if (fd_src == -1) {
		return ReturnStatus(context, false, "Can't open source image file '" + from + "': " + string(strerror(errno)));
	}

	string permission = GetParam(command, "read_only");
	// Since rascsi is running as root ensure that others can access the file
	int permissions = !strcasecmp(permission.c_str(), "true") ?
			S_IRUSR | S_IRGRP | S_IROTH : S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	int fd_dst = open(to.c_str(), O_WRONLY | O_CREAT, permissions);
	if (fd_dst == -1) {
		close(fd_src);

		return ReturnStatus(context, false, "Can't open destination image file '" + to + "': " + string(strerror(errno)));
	}

    if (sendfile(fd_dst, fd_src, 0, st.st_size) == -1) {
        close(fd_dst);
        close(fd_src);

		unlink(to.c_str());

        return ReturnStatus(context, false, "Can't copy image file '" + from + "' to '" + to + "': " + string(strerror(errno)));
	}

    close(fd_dst);
    close(fd_src);

	LOGINFO("Copied image file '%s' to '%s'", from.c_str(), to.c_str());

	return ReturnStatus(context);
}

bool RascsiImage::SetImagePermissions(const CommandContext& context, const PbCommand& command)
{
	string filename = GetParam(command, "file");
	if (filename.empty()) {
		return ReturnStatus(context, false, "Missing image filename");
	}

	if (!CheckDepth(filename)) {
		return ReturnStatus(context, false, ("Invalid folder hierarchy depth '" + filename + "'").c_str());
	}

	filename = default_image_folder + "/" + filename;
	if (!IsValidSrcFilename(filename)) {
		return ReturnStatus(context, false, "Can't modify image file '" + filename + "': Invalid name or type");
	}

	bool protect = command.operation() == PROTECT_IMAGE;

	int permissions = protect ? S_IRUSR | S_IRGRP | S_IROTH : S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	if (chmod(filename.c_str(), permissions) == -1) {
		return ReturnStatus(context, false, "Can't " + string(protect ? "protect" : "unprotect") + " image file '" + filename + "': " +
				strerror(errno));
	}

	if (protect) {
		LOGINFO("Protected image file '%s'", filename.c_str());
	}
	else {
		LOGINFO("Unprotected image file '%s'", filename.c_str());
	}

	return ReturnStatus(context);
}
