//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
//---------------------------------------------------------------------------

#include "devices/cfilesystem.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

std::filesystem::path MakeTemporaryDirectory()
{
	static std::atomic_uint counter = 0;
	const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
		std::to_string(counter++);
	return std::filesystem::temp_directory_path() / ("piscsi-cfilesystem-test-" + suffix); //NOSONAR test code
}

Human68k::namests_t MakeName(std::string_view path, std::string_view filename)
{
	Human68k::namests_t name = {};
	EXPECT_LT(path.size(), sizeof(name.path));
	memcpy(name.path, path.data(), path.size());

	const auto period = filename.rfind('.');
	const std::string_view base = period == std::string_view::npos ? filename : filename.substr(0, period);
	const std::string_view extension = period == std::string_view::npos ? "" : filename.substr(period + 1);
	EXPECT_LE(base.size(), sizeof(name.name) + sizeof(name.add));
	EXPECT_LE(extension.size(), sizeof(name.ext));

	memset(name.name, ' ', sizeof(name.name));
	memset(name.ext, ' ', sizeof(name.ext));
	memcpy(name.name, base.data(), std::min(base.size(), sizeof(name.name)));
	if (base.size() > sizeof(name.name))
		memcpy(name.add, base.data() + sizeof(name.name), base.size() - sizeof(name.name));
	memcpy(name.ext, extension.data(), extension.size());

	return name;
}

class CFileSysTest : public testing::Test { //NOSONAR test fixture state is accessed by TEST_F-derived tests
protected:
	void SetUp() override
	{
		root_ = MakeTemporaryDirectory();
		ASSERT_TRUE(std::filesystem::create_directory(root_));

		Human68k::argument_t argument = {};
		const std::string program = "windrv";
		const std::string mount_path = root_.string();
		ASSERT_LE(program.size() + mount_path.size() + 3, sizeof(argument.buf));
		memcpy(argument.buf, program.data(), program.size());
		memcpy(argument.buf + program.size() + 1, mount_path.data(), mount_path.size());

		ASSERT_EQ(1U, filesystem_.InitDevice(&argument));
	}

	void TearDown() override
	{
		filesystem_.Reset();
		std::error_code error;
		std::filesystem::remove_all(root_, error);
	}

	std::filesystem::path root_;
	CFileSys filesystem_;
};

} // namespace

TEST(CFilesystemTest, RetainsHuman68kFilenameConversion)
{
	CHostFilename filename;
	ASSERT_TRUE(filename.SetHost("README.TXT"));

	filename.ConvertHuman();

	EXPECT_TRUE(filename.isCorrect());
	EXPECT_FALSE(filename.isReduce());
	EXPECT_STREQ("README.TXT", (const char*)filename.GetHuman());
}

TEST(CFilesystemTest, RejectsOversizedHostNamesWithoutReplacingTheCurrentName)
{
	CHostFilename filename;
	ASSERT_TRUE(filename.SetHost("valid"));

	const std::string oversized(FILEPATH_MAX, 'a');
	EXPECT_FALSE(filename.SetHost(oversized.c_str()));
	EXPECT_STREQ("valid", filename.GetHost());
}

TEST(CFilesystemTest, RejectsOversizedHostPathAppends)
{
	CHostFiles files;
	ASSERT_TRUE(files.SetResult("/host/"));
	ASSERT_TRUE(files.AddResult("file"));
	EXPECT_STREQ("/host/file", files.GetPath());

	const std::string oversized(FILEPATH_MAX, 'a');
	EXPECT_FALSE(files.AddResult(oversized.c_str()));
	EXPECT_STREQ("/host/file", files.GetPath());
}

TEST(CFilesystemTest, RoundTripsJapaneseHostNamesBetweenUtf8AndShiftJis)
{
	// "日本語.txt" in UTF-8 and Shift-JIS. Use byte strings so the expected
	// Human68k name is independent of the compiler source-file encoding.
	const std::string utf8_name("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E.txt");
	const std::string shift_jis_name("\x93\xFA\x96\x7B\x8C\xEA.txt");
	const auto directory = std::filesystem::temp_directory_path() / "piscsi-cfilesystem-sjis-test"; //NOSONAR test code

	std::error_code error;
	std::filesystem::remove_all(directory, error);
	ASSERT_TRUE(std::filesystem::create_directory(directory, error)) << error.message();
	std::ofstream file(directory / utf8_name);
	ASSERT_TRUE(file.is_open());
	file.close();

	CHostPath path;
	ASSERT_TRUE(path.SetHuman((const uint8_t*)"/"));
	ASSERT_TRUE(path.SetHost((directory.string() + "/").c_str()));
	path.Refresh();

	// Refresh() receives UTF-8 from scandir(), converts it to Shift-JIS for
	// the Human68k cache, then converts the host path back to UTF-8 for stat().
	EXPECT_NE(nullptr, path.FindFilename((const uint8_t*)shift_jis_name.c_str()));

	std::filesystem::remove_all(directory, error);
}

TEST(CFilesystemTest, ConvertsNamesAndBuildsHuman68kNameStructures)
{
	CHostFilename filename;
	ASSERT_TRUE(filename.SetHost("my file+name-test.txt"));
	CFileSys options;
	const uint32_t original_option = CFileSys::GetFileOption();
	options.SetOption(original_option);
	options.SetOption(WINDRV_OPT_CONVERT_SPACE | WINDRV_OPT_CONVERT_BADCHAR | WINDRV_OPT_CONVERT_HYPHENS);
	filename.ConvertHuman();
	EXPECT_TRUE(filename.isCorrect());
	EXPECT_STREQ("my_file_name_test.txt", (const char*)filename.GetHuman());

	filename.SetEntryName();
	const auto* entry = filename.GetEntry();
	EXPECT_EQ("my_file_", std::string((const char*)entry->name, sizeof(entry->name)));
	EXPECT_STREQ("name_test", (const char*)entry->add);
	EXPECT_EQ("txt", std::string((const char*)entry->ext, sizeof(entry->ext)));

	Human68k::namests_t name = {};
	memcpy(name.path, "/DIR\x09" "CHILD/", 11);
	memcpy(name.name, "FILE    ", sizeof(name.name));
	memcpy(name.add, "LONG", 4);
	memcpy(name.ext, "TXT", sizeof(name.ext));

	std::array<uint8_t, 66> path = {};
	std::array<uint8_t, 24> full_name = {};
	name.GetCopyPath(path.data());
	name.GetCopyFilename(full_name.data());
	EXPECT_STREQ("/DIR/CHILD/", (const char*)path.data());
	EXPECT_STREQ("FILE    LONG.TXT", (const char*)full_name.data());

	options.SetOption(0);
}

TEST_F(CFileSysTest, InitializesMountedFilesystemAndHandlesControlCommands)
{
	Human68k::capacity_t capacity = {};
	EXPECT_EQ(0x7fff8000, filesystem_.GetCapacity(0, &capacity));
	EXPECT_EQ(0xffff, capacity.freearea);
	EXPECT_EQ(64, capacity.sectors);
	EXPECT_EQ(512, capacity.bytes);

	Human68k::dpb_t dpb = {};
	EXPECT_EQ(0, filesystem_.GetDPB(0, &dpb));
	EXPECT_EQ(512, dpb.sector_size);
	EXPECT_EQ(63, dpb.cluster_size);
	EXPECT_EQ(Human68k::MEDIA_REMOTE, dpb.media);

	Human68k::ctrldrive_t control = {};
	EXPECT_EQ(0x42, filesystem_.CtrlDrive(0, &control));
	EXPECT_EQ(0x42, control.status);

	Human68k::ioctrl_t ioctrl = {};
	EXPECT_EQ(0, filesystem_.Ioctrl(0, 0, &ioctrl));
	uint16_t media = 0;
	memcpy(&media, &ioctrl, sizeof(media));
	EXPECT_EQ(Human68k::MEDIA_REMOTE, media);
	EXPECT_EQ(0, filesystem_.Ioctrl(0, static_cast<uint32_t>(-1), &ioctrl));
	EXPECT_EQ("WindrvXM", std::string((const char*)ioctrl.buffer, sizeof(ioctrl.buffer)));

	EXPECT_EQ(0, filesystem_.Flush(0));
	EXPECT_EQ(0, filesystem_.Lock(0));
	EXPECT_EQ(0, filesystem_.CheckMedia(0));
}

TEST_F(CFileSysTest, EnumeratesWildcardMatchesAndReturnsVolumeLabel)
{
	std::ofstream(root_ / "alpha.bin") << "bin";
	std::ofstream(root_ / "alpha.txt") << "text";
	std::ofstream(root_ / "beta.txt") << "other";

	Human68k::files_t files = {};
	files.fatr = Human68k::AT_ARCHIVE;
	const auto alpha = MakeName("/", "alpha.???");
	ASSERT_EQ(0, filesystem_.Files(0, 0x100, &alpha, &files));
	EXPECT_STREQ("alpha.bin", (const char*)files.full);
	EXPECT_EQ(0x100U, files.sector);

	ASSERT_EQ(0, filesystem_.NFiles(0, 0x100, &files));
	EXPECT_STREQ("alpha.txt", (const char*)files.full);
	EXPECT_EQ(FS_FILENOTFND, filesystem_.NFiles(0, 0x100, &files));

	Human68k::files_t volume = {};
	volume.fatr = Human68k::AT_VOLUME;
	const auto root = MakeName("/", "");
	ASSERT_EQ(0, filesystem_.Files(0, 0x101, &root, &volume));
	EXPECT_EQ(Human68k::AT_VOLUME, volume.attr);
	EXPECT_TRUE(std::string((const char*)volume.full).starts_with("RASDRV"));
}

TEST_F(CFileSysTest, CreatesReadsWritesSeeksTruncatesAndTimestampsFiles)
{
	const auto file = MakeName("/", "data.bin");
	Human68k::fcb_t fcb = {};
	ASSERT_EQ(0, filesystem_.Create(0, 0x200, &file, &fcb, 0, false));
	EXPECT_EQ(Human68k::OP_FULL, fcb.mode & Human68k::OP_MASK);

	const std::array<uint8_t, 5> content = {'h', 'e', 'l', 'l', 'o'};
	EXPECT_EQ(5, filesystem_.Write(0x200, &fcb, content.data(), content.size()));
	EXPECT_EQ(5U, fcb.fileptr);
	EXPECT_EQ(5U, fcb.size);
	EXPECT_EQ(0, filesystem_.Seek(0x200, &fcb, (uint32_t)Human68k::seek_t::SK_BEGIN, 0));

	std::array<uint8_t, 5> read = {};
	EXPECT_EQ(5, filesystem_.Read(0x200, &fcb, read.data(), read.size()));
	EXPECT_EQ(content, read);
	EXPECT_EQ(3, filesystem_.Seek(0x200, &fcb, (uint32_t)Human68k::seek_t::SK_BEGIN, 3));
	EXPECT_EQ(0, filesystem_.Write(0x200, &fcb, nullptr, 0));
	EXPECT_EQ(3U, fcb.size);

	const uint16_t date = (46U << 9) | (1U << 5) | 2U;
	const uint16_t time = (3U << 11) | (4U << 5) | 2U;
	EXPECT_EQ(0U, filesystem_.TimeStamp(0, 0x200, &fcb, (uint32_t(date) << 16) | time));
	EXPECT_EQ(date, fcb.date);
	EXPECT_EQ(time, fcb.time);
	EXPECT_EQ(0, filesystem_.Close(0, 0x200, &fcb));

	Human68k::fcb_t reopened = {};
	reopened.mode = Human68k::OP_READ;
	ASSERT_EQ(0, filesystem_.Open(0, 0x201, &file, &reopened));
	EXPECT_EQ(3U, reopened.size);
	std::array<uint8_t, 4> truncated = {};
	EXPECT_EQ(3, filesystem_.Read(0x201, &reopened, truncated.data(), truncated.size()));
	EXPECT_EQ("hel", std::string((const char*)truncated.data(), 3));
	EXPECT_EQ(0, filesystem_.Close(0, 0x201, &reopened));
}

TEST_F(CFileSysTest, CreatesRenamesDeletesAndRemovesDirectories)
{
	std::ofstream(root_ / "source.txt") << "source";
	const auto root_docs = MakeName("/", "docs");
	ASSERT_EQ(0, filesystem_.MakeDir(0, &root_docs));
	EXPECT_TRUE(std::filesystem::is_directory(root_ / "docs"));

	const auto source = MakeName("/", "source.txt");
	const auto destination = MakeName("/docs/", "moved.txt");
	ASSERT_EQ(0, filesystem_.Rename(0, &source, &destination));
	EXPECT_FALSE(std::filesystem::exists(root_ / "source.txt"));
	EXPECT_TRUE(std::filesystem::exists(root_ / "docs" / "moved.txt"));

	ASSERT_EQ(0, filesystem_.Delete(0, &destination));
	EXPECT_FALSE(std::filesystem::exists(root_ / "docs" / "moved.txt"));
	EXPECT_EQ(0, filesystem_.RemoveDir(0, &root_docs));
	EXPECT_FALSE(std::filesystem::exists(root_ / "docs"));
}

TEST_F(CFileSysTest, ReturnsProtocolErrorsForInvalidUnitsAndMissingHandles)
{
	const auto missing = MakeName("/", "missing.bin");
	Human68k::fcb_t fcb = {};
	std::array<uint8_t, 1> buffer = {};

	EXPECT_EQ(FS_FATAL_INVALIDUNIT, filesystem_.CheckDir(CFileSys::DriveMax, &missing));
	EXPECT_EQ(FS_INVALIDFUNC, filesystem_.CheckDir(1, &missing));
	EXPECT_EQ(FS_FILENOTFND, filesystem_.Open(0, 0x300, &missing, &fcb));
	EXPECT_EQ(FS_INVALIDPRM, filesystem_.Close(0, 0x300, &fcb));
	EXPECT_EQ(FS_NOTOPENED, filesystem_.Read(0x300, &fcb, buffer.data(), buffer.size()));
	EXPECT_EQ(FS_NOTOPENED, filesystem_.Write(0x300, &fcb, buffer.data(), buffer.size()));
	EXPECT_EQ(FS_NOTOPENED, filesystem_.Seek(0x300, &fcb, (uint32_t)Human68k::seek_t::SK_BEGIN, 0));

	Human68k::files_t files = {};
	EXPECT_EQ(FS_FATAL_MEDIAOFFLINE, filesystem_.Files(1, 0x301, &missing, &files));
}

TEST_F(CFileSysTest, ReadsPseudoDirectoryAndFileSectors)
{
	const std::array<uint8_t, 6> content = {'s', 'e', 'c', 't', 'o', 'r'};
	std::ofstream host_file(root_ / "sector.bin", std::ios::binary);
	host_file.write((const char*)content.data(), content.size());
	host_file.close();

	Human68k::files_t files = {};
	files.fatr = Human68k::AT_ARCHIVE;
	const auto file = MakeName("/", "sector.bin");
	constexpr uint32_t key = 0x400;
	ASSERT_EQ(0, filesystem_.Files(0, key, &file, &files));

	std::array<uint8_t, 0x200> directory_sector = {};
	ASSERT_EQ(0, filesystem_.DiskRead(0, directory_sector.data(), key, 1));
	Human68k::dirent_t entry = {};
	memcpy(&entry, directory_sector.data(), sizeof(entry));
	EXPECT_EQ("sector  ", std::string((const char*)entry.name, sizeof(entry.name)));
	EXPECT_EQ(2, entry.cluster);

	Human68k::dpb_t dpb = {};
	ASSERT_EQ(0, filesystem_.GetDPB(0, &dpb));
	const uint32_t file_sector = (entry.cluster - 2) * (dpb.cluster_size + 1) + dpb.data_sector;
	std::array<uint8_t, 0x200> file_sector_data = {};
	ASSERT_EQ(0, filesystem_.DiskRead(0, file_sector_data.data(), file_sector, 1));
	EXPECT_TRUE(std::equal(content.begin(), content.end(), file_sector_data.begin()));
}

TEST_F(CFileSysTest, RetainsPseudoSectorPathAfterSearchStateIsReused)
{
	std::ofstream(root_ / "alpha.bin", std::ios::binary) << "alpha";
	std::ofstream(root_ / "beta.bin", std::ios::binary) << "beta";

	Human68k::files_t files = {};
	files.fatr = Human68k::AT_ARCHIVE;
	constexpr uint32_t key = 0x500;
	const auto alpha = MakeName("/", "alpha.bin");
	ASSERT_EQ(0, filesystem_.Files(0, key, &alpha, &files));

	std::array<uint8_t, 0x200> directory_sector = {};
	ASSERT_EQ(0, filesystem_.DiskRead(0, directory_sector.data(), key, 1));
	Human68k::dirent_t entry = {};
	memcpy(&entry, directory_sector.data(), sizeof(entry));

	// A subsequent search with the same key replaces its CHostFiles state. The
	// pseudo-sector must still address the file named by the first entry.
	const auto beta = MakeName("/", "beta.bin");
	ASSERT_EQ(0, filesystem_.Files(0, key, &beta, &files));

	Human68k::dpb_t dpb = {};
	ASSERT_EQ(0, filesystem_.GetDPB(0, &dpb));
	const uint32_t file_sector = (entry.cluster - 2) * (dpb.cluster_size + 1) + dpb.data_sector;
	std::array<uint8_t, 0x200> file_sector_data = {};
	ASSERT_EQ(0, filesystem_.DiskRead(0, file_sector_data.data(), file_sector, 1));
	EXPECT_EQ("alpha", std::string((const char*)file_sector_data.data(), 5));
}

TEST_F(CFileSysTest, RefusesToEvictActiveFileSearches)
{
	std::ofstream(root_ / "entry.bin", std::ios::binary) << "entry";
	const auto entry = MakeName("/", "entry.bin");

	for (uint32_t index = 0; index < XM6_HOST_FILES_MAX; index++) {
		Human68k::files_t files = {};
		files.fatr = Human68k::AT_ARCHIVE;
		ASSERT_EQ(0, filesystem_.Files(0, 0x600 + index, &entry, &files));
	}

	Human68k::files_t overflow = {};
	overflow.fatr = Human68k::AT_ARCHIVE;
	EXPECT_EQ(FS_OUTOFMEM, filesystem_.Files(0, 0x700, &entry, &overflow));

	// Finishing one search releases exactly one slot for reuse.
	Human68k::files_t completed = {};
	EXPECT_EQ(FS_FILENOTFND, filesystem_.NFiles(0, 0x600, &completed));
	EXPECT_EQ(0, filesystem_.Files(0, 0x700, &entry, &overflow));
}
