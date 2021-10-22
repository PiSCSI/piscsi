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
#include <sstream>
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
}

string RascsiImage::SetDefaultImageFolder(const string& f)
{
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

bool RascsiImage::CreateImage(int fd, const PbCommand& command)
{
	string filename = GetParam(command, "file");
	if (filename.empty()) {
		return ReturnStatus(fd, false, "Can't create image file: Missing image filename");
	}
	if (filename.find('/') != string::npos) {
		return ReturnStatus(fd, false, "Can't create image file '" + filename + "': Filename must not contain a path");
	}
	filename = default_image_folder + "/" + filename;
	if (!IsValidDstFilename(filename)) {
		return ReturnStatus(fd, false, "Can't create image file: '" + filename + "': File already exists");
	}

	const string size = GetParam(command, "size");
	if (size.empty()) {
		return ReturnStatus(fd, false, "Can't create image file '" + filename + "': Missing image size");
	}

	off_t len;
	try {
		len = stoul(size);
	}
	catch(const invalid_argument& e) {
		return ReturnStatus(fd, false, "Can't create image file '" + filename + "': Invalid file size " + size);
	}
	catch(const out_of_range& e) {
		return ReturnStatus(fd, false, "Can't create image file '" + filename + "': Invalid file size " + size);
	}
	if (len < 512 || (len & 0x1ff)) {
		ostringstream error;
		error << "Invalid image file size " << len;
		return ReturnStatus(fd, false, error.str());
	}

	struct stat st;
	if (!stat(filename.c_str(), &st)) {
		return ReturnStatus(fd, false, "Can't create image file '" + filename + "': File already exists");
	}

	string permission = GetParam(command, "read_only");
	// Since rascsi is running as root ensure that others can access the file
	int permissions = !strcasecmp(permission.c_str(), "true") ?
			S_IRUSR | S_IRGRP | S_IROTH : S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	int image_fd = open(filename.c_str(), O_CREAT|O_WRONLY, permissions);
	if (image_fd == -1) {
		return ReturnStatus(fd, false, "Can't create image file '" + filename + "': " + string(strerror(errno)));
	}

	if (fallocate(image_fd, 0, 0, len) == -1) {
		close(image_fd);

		unlink(filename.c_str());

		return ReturnStatus(fd, false, "Can't allocate space for image file '" + filename + "': " + string(strerror(errno)));
	}

	close(image_fd);

	ostringstream msg;
	msg << "Created " << (permissions & S_IWUSR ? "": "read-only ") << "image file '" << filename + "' with a size of " << len << " bytes";
	LOGINFO("%s", msg.str().c_str());

	return ReturnStatus(fd);
}

bool RascsiImage::DeleteImage(int fd, const PbCommand& command)
{
	string filename = GetParam(command, "file");
	if (filename.empty()) {
		return ReturnStatus(fd, false, "Missing image filename");
	}

	if (!IsValidDstFilename(filename)) {
		return ReturnStatus(fd, false, "Can't delete image  file '" + filename + "': File already exists");
	}

	if (filename.find('/') != string::npos) {
		return ReturnStatus(fd, false, "The image filename '" + filename + "' must not contain a path");
	}

	filename = default_image_folder + "/" + filename;

	int id;
	int unit;
	Filepath filepath;
	filepath.SetPath(filename.c_str());
	if (FileSupport::GetIdsForReservedFile(filepath, id, unit)) {
		ostringstream msg;
		msg << "Can't delete image file '" << filename << "', it is used by device ID " << id << ", unit " << unit;
		return ReturnStatus(fd, false, msg.str());
	}

	if (unlink(filename.c_str())) {
		return ReturnStatus(fd, false, "Can't delete image file '" + filename + "': " + string(strerror(errno)));
	}

	LOGINFO("Deleted image file '%s'", filename.c_str());

	return ReturnStatus(fd);
}

bool RascsiImage::RenameImage(int fd, const PbCommand& command)
{
	string from = GetParam(command, "from");
	if (from.empty()) {
		return ReturnStatus(fd, false, "Can't rename image file: Missing source filename");
	}
	if (from.find('/') != string::npos) {
		return ReturnStatus(fd, false, "The source filename '" + from + "' must not contain a path");
	}
	from = default_image_folder + "/" + from;
	if (!IsValidSrcFilename(from)) {
		return ReturnStatus(fd, false, "Can't rename image file: '" + from + "': Invalid name or type");
	}

	string to = GetParam(command, "to");
	if (to.empty()) {
		return ReturnStatus(fd, false, "Can't rename image file '" + from + "': Missing destination filename");
	}
	if (to.find('/') != string::npos) {
		return ReturnStatus(fd, false, "The destination filename '" + to + "' must not contain a path");
	}
	to = default_image_folder + "/" + to;
	if (!IsValidDstFilename(to)) {
		return ReturnStatus(fd, false, "Can't rename image file '" + from + "' to '" + to + "': File already exists");
	}

	if (rename(from.c_str(), to.c_str())) {
		return ReturnStatus(fd, false, "Can't rename image file '" + from + "' to '" + to + "': " + string(strerror(errno)));
	}

	LOGINFO("Renamed image file '%s' to '%s'", from.c_str(), to.c_str());

	return ReturnStatus(fd);
}

bool RascsiImage::CopyImage(int fd, const PbCommand& command)
{
	string from = GetParam(command, "from");
	if (from.empty()) {
		return ReturnStatus(fd, false, "Can't copy image file: Missing source filename");
	}
	if (from.find('/') != string::npos) {
		return ReturnStatus(fd, false, "The source filename '" + from + "' must not contain a path");
	}
	from = default_image_folder + "/" + from;
	if (!IsValidSrcFilename(from)) {
		return ReturnStatus(fd, false, "Can't copy image file: '" + from + "': Invalid name or type");
	}

	string to = GetParam(command, "to");
	if (to.empty()) {
		return ReturnStatus(fd, false, "Can't copy image file '" + from + "': Missing destination filename");
	}
	if (to.find('/') != string::npos) {
		return ReturnStatus(fd, false, "The destination filename '" + to + "' must not contain a path");
	}
	to = default_image_folder + "/" + to;
	if (!IsValidDstFilename(to)) {
		return ReturnStatus(fd, false, "Can't copy image file '" + from + "' to '" + to + "': File already exists");
	}

	struct stat st;
    if (lstat(from.c_str(), &st)) {
    	return ReturnStatus(fd, false, "Can't access source image file '" + from + "': " + string(strerror(errno)));
    }

    // Symbolic links need a special handling
	if ((st.st_mode & S_IFMT) == S_IFLNK) {
		if (symlink(filesystem::read_symlink(from).c_str(), to.c_str())) {
	    	return ReturnStatus(fd, false, "Can't copy symlink '" + from + "': " + string(strerror(errno)));
		}

		LOGINFO("Copied symlink '%s' to '%s'", from.c_str(), to.c_str());

		return ReturnStatus(fd);
	}

	int fd_src = open(from.c_str(), O_RDONLY, 0);
	if (fd_src == -1) {
		return ReturnStatus(fd, false, "Can't open source image file '" + from + "': " + string(strerror(errno)));
	}

	string permission = GetParam(command, "read_only");
	// Since rascsi is running as root ensure that others can access the file
	int permissions = !strcasecmp(permission.c_str(), "true") ?
			S_IRUSR | S_IRGRP | S_IROTH : S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	int fd_dst = open(to.c_str(), O_WRONLY | O_CREAT, permissions);
	if (fd_dst == -1) {
		close(fd_src);

		return ReturnStatus(fd, false, "Can't open destination image file '" + to + "': " + string(strerror(errno)));
	}

    if (sendfile(fd_dst, fd_src, 0, st.st_size) == -1) {
        close(fd_dst);
        close(fd_src);

		unlink(to.c_str());

        return ReturnStatus(fd, false, "Can't copy image file '" + from + "' to '" + to + "': " + string(strerror(errno)));
	}

    close(fd_dst);
    close(fd_src);

	LOGINFO("Copied image file '%s' to '%s'", from.c_str(), to.c_str());

	return ReturnStatus(fd);
}

bool RascsiImage::SetImagePermissions(int fd, const PbCommand& command)
{
	string filename = GetParam(command, "file");
	if (filename.empty()) {
		return ReturnStatus(fd, false, "Missing image filename");
	}
	if (filename.find('/') != string::npos) {
		return ReturnStatus(fd, false, "The image filename '" + filename + "' must not contain a path");
	}
	filename = default_image_folder + "/" + filename;
	if (!IsValidSrcFilename(filename)) {
		return ReturnStatus(fd, false, "Can't modify image file '" + filename + "': Invalid name or type");
	}

	bool protect = command.operation() == PROTECT_IMAGE;

	int permissions = protect ? S_IRUSR | S_IRGRP | S_IROTH : S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	if (chmod(filename.c_str(), permissions) == -1) {
		ostringstream error;
		error << "Can't " << (protect ? "protect" : "unprotect") << " image file '" << filename << "': " << strerror(errno);
		return ReturnStatus(fd, false, error.str());
	}

	if (protect) {
		LOGINFO("Protected image file '%s'", filename.c_str());
	}
	else {
		LOGINFO("Unprotected image file '%s'", filename.c_str());
	}

	return ReturnStatus(fd);
}
