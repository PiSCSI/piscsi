//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	Imported sava's bugfix patch(in RASDRV DOS edition).
//
//  [ Host File System for the X68000 ]
//
//  Note: This functionality is specific to the X68000
//   operating system.
//   It is highly unlikely that this will work for other
//   platforms.
//
//---------------------------------------------------------------------------

#include "os.h"
#include "log.h"
#include "filepath.h"
#include "cfilesystem.h"
#include <dirent.h>
#include <iconv.h>
#include <utime.h>

#define ARRAY_SIZE(x) (sizeof(x)/(sizeof(x[0])))

//---------------------------------------------------------------------------
//
//  Kanji code conversion
//
//---------------------------------------------------------------------------
static const int IC_BUF_SIZE = 1024;
static char convert_buf[IC_BUF_SIZE];
#define CONVERT(src, dest, inbuf, outbuf, outsize) \
	convert(src, dest, (char *)inbuf, outbuf, outsize)
static void convert(char const *src, char const *dest,
	char *inbuf, char *outbuf, size_t outsize)
{
	*outbuf = '\0';
	size_t in = strlen(inbuf);
	size_t out = outsize - 1;

	iconv_t cd = iconv_open(dest, src);
	if (cd == (iconv_t)-1) {
		return;
	}

	if (size_t ret = iconv(cd, &inbuf, &in, &outbuf, &out); ret == (size_t)-1) {
		return;
	}

	iconv_close(cd);
	*outbuf = '\0';
}

//---------------------------------------------------------------------------
//
// SJIS->UTF8 conversion
//
//---------------------------------------------------------------------------
static char* SJIS2UTF8(const char *sjis, char *utf8, size_t bufsize)
{
	CONVERT("SJIS", "UTF-8", sjis, utf8, bufsize);
	return convert_buf;
}

//---------------------------------------------------------------------------
//
// UTF8->SJIS conversion
//
//---------------------------------------------------------------------------
static char* UTF82SJIS(const char *utf8, char *sjis, size_t bufsize)
{
	CONVERT("UTF-8", "SJIS", utf8, sjis, bufsize);
	return convert_buf;
}

//---------------------------------------------------------------------------
//
// SJIS->UTF8 conversion (simplified versoin)
//
//---------------------------------------------------------------------------
static char* S2U(const char *sjis)
{
	SJIS2UTF8(sjis, convert_buf, IC_BUF_SIZE);
	return convert_buf;
}

//---------------------------------------------------------------------------
//
// UTF8->SJIS conversion (simplified version)
//
//---------------------------------------------------------------------------
static char* U2S(const char *utf8)
{
	UTF82SJIS(utf8, convert_buf, IC_BUF_SIZE);
	return convert_buf;
}

//---------------------------------------------------------------------------
//
/// Get path name
///
/// From the structure used in Human68k namests, get the Human68k path name.
/// A 66 byte buffer is required for writing.
//
//---------------------------------------------------------------------------
void Human68k::namests_t::GetCopyPath(BYTE* szPath) const
{
	assert(szPath);

	BYTE* p = szPath;
	for (BYTE c : path) {
		if (c == '\0')
			break;
		if (c == 0x09) {
			c = '/';
		}
		*p++ = c;
	}

	*p = '\0';
}

//---------------------------------------------------------------------------
//
/// Get file name
///
/// From the structure used in Human68k namests, get the Human68k file name.
/// A 23 byte buffer is required for writing.
//
//---------------------------------------------------------------------------
void Human68k::namests_t::GetCopyFilename(BYTE* szFilename) const
{
	assert(szFilename);

	size_t i;
	BYTE* p = szFilename;

	// Transfer the base file name
	for (i = 0; i < 8; i++) {
		BYTE c = name[i];
		if (c == ' ') {
			// Check that the file name continues after a space is detected
			/// TODO: Should change this function to be compatible with 8+3 chars and TwentyOne
			// Continue if add[0] is a valid character
			if (add[0] != '\0')
				goto next_name;
			// Continue if a non-space character exists after name[i]
			for (size_t j = i + 1; j < 8; j++) {
				if (name[j] != ' ')
					goto next_name;
			}
			// Exit if the file name ends
			break;
		}
	next_name:
		*p++ = c;
	}
	// At this point, the number of read characters becomes i >= 8

	// If the body of the file name exceeds 8 characters, add the extraneous part
	if (i >= 8) {
		// Transfer the extraneous part
		for (i = 0; i < 10; i++) {
			BYTE c = add[i];
			if (c == '\0')
				break;
			*p++ = c;
		}
		// At this point, the number of read characters becomes i >= 10
	}

	// Transfer the file extension if it exists
	if (ext[0] != ' ' || ext[1] != ' ' || ext[2] != ' ') {
		*p++ = '.';
		for (i = 0; i < 3; i++) {
			BYTE c = ext[i];
			if (c == ' ') {
				// Check that the file extension continues after a space is detected
				/// TODO: Should change this function to be compatible with 8+3 chars and TwentyOne
				// Continue if a non-space character exists after ext[i]
				for (size_t j = i + 1; j < 3; j++) {
					if (ext[j] != ' ')
						goto next_ext;
				}
				// If the extension ends, the transfer ends
				break;
			}
		next_ext:
			*p++ = c;
		}
		//  When all the characters are read, here i >= 3
	}

	//  Add sentinel
	*p = '\0';
}

//===========================================================================
//
//	Host side drive
//
//===========================================================================

CHostDrv::~CHostDrv()
{
	CHostPath* p;
	while ((p = (CHostPath*)m_cRing.Next()) != &m_cRing) {
		delete p;
		assert(m_nRing);
		m_nRing--;
	}

	//  Confirm that the entity does not exist (just in case)
	assert(m_cRing.Next() == &m_cRing);
	assert(m_cRing.Prev() == &m_cRing);
	assert(m_nRing == 0);
}

//---------------------------------------------------------------------------
//
// Initialization (device boot and load)
//
//---------------------------------------------------------------------------
void CHostDrv::Init(const TCHAR* szBase, uint32_t nFlag)
{
	assert(szBase);
	assert(strlen(szBase) < FILEPATH_MAX);
	assert(!m_bWriteProtect);
	assert(!m_bEnable);
	assert(m_capCache.sectors == 0);
	assert(!m_bVolumeCache);
	assert(m_szVolumeCache[0] == '\0');

	// Confirm that the entity does not exist (just in case)
	assert(m_cRing.Next() == &m_cRing);
	assert(m_cRing.Prev() == &m_cRing);
	assert(m_nRing == 0);

	m_capCache.sectors = 0;

	// Receive parameters
	if (nFlag & FSFLAG_WRITE_PROTECT)
		m_bWriteProtect = true;
	strcpy(m_szBase, szBase);

	// Remove the last path delimiter in the base path
	// @warning needs to be modified when using Unicode
	TCHAR* pClear = nullptr;
	TCHAR* p = m_szBase;
	for (;;) {
		TCHAR c = *p;
		if (c == '\0')
			break;
		if (c == '/' || c == '\\') {
			pClear = p;
		} else {
			pClear = nullptr;
		}
		if ((c <= (TCHAR)0x9F) || (TCHAR)0xE0 <= c) {	// To be precise: 0x81~0x9F 0xE0~0xEF
			p++;
			if (*p == '\0')
				break;
		}
		p++;
	}
	if (pClear)
		*pClear = '\0';

	// Status update
	m_bEnable = true;
}

//---------------------------------------------------------------------------
//
// Media check
//
//---------------------------------------------------------------------------
bool CHostDrv::isMediaOffline() const
{
	// Offline status check
	return !m_bEnable;
}

//---------------------------------------------------------------------------
//
// Get media bytes
//
//---------------------------------------------------------------------------
BYTE CHostDrv::GetMediaByte() const
{
	return Human68k::MEDIA_REMOTE;
}

//---------------------------------------------------------------------------
//
// Get drive status
//
//---------------------------------------------------------------------------
uint32_t CHostDrv::GetStatus() const
{
	return 0x40 | (m_bEnable ? (m_bWriteProtect ? 0x08 : 0) | 0x02 : 0);
}

//---------------------------------------------------------------------------
//
// Media status settings
//
//---------------------------------------------------------------------------
void CHostDrv::SetEnable(bool bEnable)
{
	m_bEnable = bEnable;

	if (!bEnable) {
		// Clear cache
		m_capCache.sectors = 0;
		m_bVolumeCache = false;
		m_szVolumeCache[0] = '\0';
	}
}

//---------------------------------------------------------------------------
//
// Media change check
//
//---------------------------------------------------------------------------
bool CHostDrv::CheckMedia()
{
	// Status update
	Update();
	if (!m_bEnable)
		CleanCache();

	return m_bEnable;
}

//---------------------------------------------------------------------------
//
// Media status update
//
//---------------------------------------------------------------------------
void CHostDrv::Update()
{
	// Considered as media insertion
	// Media status reflected
	SetEnable(true);
}

//---------------------------------------------------------------------------
//
// Eject
//
//---------------------------------------------------------------------------
void CHostDrv::Eject()
{
	// Media discharge
	CleanCache();
	SetEnable(false);

	// Status update
	Update();
}

//---------------------------------------------------------------------------
//
// Get volume label
//
//---------------------------------------------------------------------------
void CHostDrv::GetVolume(TCHAR* szLabel)
{
	assert(szLabel);
	assert(m_bEnable);

	// Get volume label
	strcpy(m_szVolumeCache, "RASDRV ");
	if (m_szBase[0]) {
		strcat(m_szVolumeCache, m_szBase);
	} else {
		strcat(m_szVolumeCache, "/");
	}

	// Cache update
	m_bVolumeCache = true;

	// Transfer content
	strcpy(szLabel, m_szVolumeCache);
}

//---------------------------------------------------------------------------
//
/// Get volume label from cache
///
/// Transfer the cached volume label information.
/// Return true if the cache contents are valid.
//
//---------------------------------------------------------------------------
bool CHostDrv::GetVolumeCache(TCHAR* szLabel) const
{
	assert(szLabel);

	// Transfer contents
	strcpy(szLabel, m_szVolumeCache);

	return m_bVolumeCache;
}

uint32_t CHostDrv::GetCapacity(Human68k::capacity_t* pCapacity)
{
	assert(pCapacity);
	assert(m_bEnable);

	uint32_t nFree = 0x7FFF8000;
	uint32_t freearea;
	uint32_t clusters;
	uint32_t sectors;

	freearea = 0xFFFF;
	clusters = 0xFFFF;
	sectors = 64;

	// Estimated parameter range
	assert(freearea <= 0xFFFF);
	assert(clusters <= 0xFFFF);
	assert(sectors <= 64);

	// Update cache
	m_capCache.freearea = (uint16_t)freearea;
	m_capCache.clusters = (uint16_t)clusters;
	m_capCache.sectors = (uint16_t)sectors;
	m_capCache.bytes = 512;

	// Transfer contents
	memcpy(pCapacity, &m_capCache, sizeof(m_capCache));

	return nFree;
}

//---------------------------------------------------------------------------
//
/// Get capacity from the cache
///
/// Transfer the capacity data stored in cache.
/// Return true if the contents of the cache are valid.
//
//---------------------------------------------------------------------------
bool CHostDrv::GetCapacityCache(Human68k::capacity_t* pCapacity) const
{
	assert(pCapacity);

	// Transfer contents
	memcpy(pCapacity, &m_capCache, sizeof(m_capCache));

	return m_capCache.sectors != 0;
}

//---------------------------------------------------------------------------
//
/// Update all cache
//
//---------------------------------------------------------------------------
void CHostDrv::CleanCache() const
{
	for (auto p = (CHostPath*)m_cRing.Next(); p != &m_cRing;) {
		p->Release();
		p = (CHostPath*)p->Next();
	}
}

//---------------------------------------------------------------------------
//
/// Update the cache for the specified path
//
//---------------------------------------------------------------------------
void CHostDrv::CleanCache(const BYTE* szHumanPath)
{
	assert(szHumanPath);

	CHostPath* p = FindCache(szHumanPath);
	if (p) {
		p->Restore();
		p->Release();
	}
}

//---------------------------------------------------------------------------
//
/// Update the cache below and including the specified path
//
//---------------------------------------------------------------------------
void CHostDrv::CleanCacheChild(const BYTE* szHumanPath) const
{
	assert(szHumanPath);

	auto p = (CHostPath*)m_cRing.Next();
	while (p != &m_cRing) {
		if (p->isSameChild(szHumanPath))
			p->Release();
		p = (CHostPath*)p->Next();
	}
}

//---------------------------------------------------------------------------
//
/// Delete the cache for the specified path
//
//---------------------------------------------------------------------------
void CHostDrv::DeleteCache(const BYTE* szHumanPath)
{
	auto p = FindCache(szHumanPath);
	if (p) {
		delete p;
		assert(m_nRing);
		m_nRing--;
	}
}

//---------------------------------------------------------------------------
//
/// Check if the specified path is cached
///
/// Check if whether it is a perfect match with the cache buffer, and return the name if found.
/// File names are excempted.
/// Make sure to lock from the top.
//
//---------------------------------------------------------------------------
CHostPath* CHostDrv::FindCache(const BYTE* szHuman)
{
	assert(szHuman);

	// Find something that matches perfectly with either of the stored file names
	for (auto p = (CHostPath*)m_cRing.Next(); p != &m_cRing;) {
		if (p->isSameHuman(szHuman))
			return p;
		p = (CHostPath*)p->Next();
	}

	return nullptr;
}

//---------------------------------------------------------------------------
//
/// Get the host side name from cached data.
///
/// Confirm if the path is cached. If not, throw an error.
/// Carry out an update check on found cache. If an update is needed, throw an error.
/// Make sure to lock from the top.
//
//---------------------------------------------------------------------------
CHostPath* CHostDrv::CopyCache(CHostFiles* pFiles)
{
	assert(pFiles);
	assert(strlen((const char*)pFiles->GetHumanPath()) < HUMAN68K_PATH_MAX);

	// Find in cache
	CHostPath* pPath = FindCache(pFiles->GetHumanPath());
	if (pPath == nullptr) {
		return nullptr;	// Error: No cache
	}

	// Move to the beginning of the ring
	pPath->Insert(&m_cRing);

	// Cache update check
	if (pPath->isRefresh()) {
		return nullptr;	// Error: Cache update is required
	}

	// Store the host side path
	pFiles->SetResult(pPath->GetHost());

	return pPath;
}

//---------------------------------------------------------------------------
//
/// Get all the data required for a host side name structure
///
/// File names can be abbreviated. (Normally not selected)
/// Make sure to lock from the top.
/// Be careful not to append the path separator char to the end of the base path.
/// Initiate VM threads when there is a chance of multiple file accesses.
///
/// How to use:
/// When CopyCache() throws an error execute MakeCache(). It ensures you get a correct host side path.
///
/// Split all file names and path names.
/// Confirm that it's cached from the top level directory down.
/// If it's cached, do a destruction check. If it was destroyed, treat it as uncached.
/// If it isn't cached, build cache.
/// Exit after processing all directory and file names in order.
/// Make it nullptr is an error is thrown.
//
//---------------------------------------------------------------------------
CHostPath* CHostDrv::MakeCache(CHostFiles* pFiles)
{
	assert(pFiles);
	assert(strlen((const char*)pFiles->GetHumanPath()) < HUMAN68K_PATH_MAX);

	assert(m_szBase);
	assert(strlen(m_szBase) < FILEPATH_MAX);

	BYTE szHumanPath[HUMAN68K_PATH_MAX];	// Path names are entered in order from the route
	szHumanPath[0] = '\0';
	size_t nHumanPath = 0;

	TCHAR szHostPath[FILEPATH_MAX];
	strcpy(szHostPath, m_szBase);
	size_t nHostPath = strlen(szHostPath);

	CHostPath* pPath;
	const BYTE* p = pFiles->GetHumanPath();
	for (;;) {
		// Add path separators
		if (nHumanPath + 1 >= HUMAN68K_PATH_MAX)
			return nullptr;				// Error: The Human68k path is too long
		szHumanPath[nHumanPath++] = '/';
		szHumanPath[nHumanPath] = '\0';
		if (nHostPath + 1 >= FILEPATH_MAX)
			return nullptr;				// Error: The host side path is too long
		szHostPath[nHostPath++] = '/';
		szHostPath[nHostPath] = '\0';

		// Insert one file
		BYTE szHumanFilename[24];		// File name part
		p = SeparateCopyFilename(p, szHumanFilename);
		if (p == nullptr)
			return nullptr;				// Error: Failed to read file name
		size_t n = strlen((const char*)szHumanFilename);
		if (nHumanPath + n >= HUMAN68K_PATH_MAX)
			return nullptr;				// Error: The Human68k path is too long

		// Is the relevant path cached?
		pPath = FindCache(szHumanPath);
		if (pPath == nullptr) {
			// Check for max number of cache
			if (m_nRing >= XM6_HOST_DIRENTRY_CACHE_MAX) {
				// Destroy the oldest cache and reuse it
				pPath = (CHostPath*)m_cRing.Prev();
				pPath->Clean();			// Release all files. Release update check handlers.
			} else {
				// Register new
				pPath = new CHostPath;
				assert(pPath);
				m_nRing++;
			}
			pPath->SetHuman(szHumanPath);
			pPath->SetHost(szHostPath);

			// Update status
			pPath->Refresh();
		}

		// Cache update check
		if (pPath->isRefresh()) {
			Update();

			// Update status
			pPath->Refresh();
		}

		// Into the beginning of the ring
		pPath->Insert(&m_cRing);

		// Exit if there is not file name
		if (n == 0)
			break;

		// Find the next path
		// Confirm if directory from the middle of the path
		const CHostFilename* pFilename;
		if (*p != '\0')
			pFilename = pPath->FindFilename(szHumanFilename, Human68k::AT_DIRECTORY);
		else
			pFilename = pPath->FindFilename(szHumanFilename);
		if (pFilename == nullptr)
			return nullptr;				// Error: Could not find path or file names in the middle

		// Link path name
		strcpy((char*)szHumanPath + nHumanPath, (const char*)szHumanFilename);
		nHumanPath += n;

		n = strlen(pFilename->GetHost());
		if (nHostPath + n >= FILEPATH_MAX)
			return nullptr;				// Error: Host side path is too long
		strcpy(szHostPath + nHostPath, pFilename->GetHost());
		nHostPath += n;

		// PLEASE CONTINUE
		if (*p == '\0')
			break;
	}

	// Store the host side path name
	pFiles->SetResult(szHostPath);

	return pPath;
}

//---------------------------------------------------------------------------
//
/// Find host side name (path name + file name (can be abbeviated) + attribute)
///
/// Set all Human68k parameters once more.
//
//---------------------------------------------------------------------------
bool CHostDrv::Find(CHostFiles* pFiles)
{
	assert(pFiles);

	// Get path name and build cache
	const CHostPath* pPath = CopyCache(pFiles);
	if (pPath == nullptr) {
		pPath = MakeCache(pFiles);
		if (pPath == nullptr) {
			CleanCache();
			return false;	// Error: Failed to build cache
		}
	}

	// Store host side path
	pFiles->SetResult(pPath->GetHost());

	// Exit if only path name
	if (pFiles->isPathOnly()) {
		return true;		// Normal exit: only path name
	}

	// Find file name
	const CHostFilename* pFilename = pFiles->Find(pPath);
	if (pFilename == nullptr) {
		return false;		// Error: Could not get file name
	}

	// Store the Human68k side search results
	pFiles->SetEntry(pFilename);

	// Store the host side full path name
	pFiles->AddResult(pFilename->GetHost());

	return true;
}

//===========================================================================
//
//	Directory entry: File name
//
//===========================================================================

//---------------------------------------------------------------------------
//
/// Set host side name
//
//---------------------------------------------------------------------------
void CHostFilename::SetHost(const TCHAR* szHost)
{
	assert(szHost);
	assert(strlen(szHost) < FILEPATH_MAX);

	strcpy(m_szHost, szHost);
}

//---------------------------------------------------------------------------
//
/// Copy the Human68k file name elements
//
//---------------------------------------------------------------------------
BYTE* CHostFilename::CopyName(BYTE* pWrite, const BYTE* pFirst, const BYTE* pLast)	// static
{
	assert(pWrite);
	assert(pFirst);
	assert(pLast);

	for (const BYTE* p = pFirst; p < pLast; p++) {
		*pWrite++ = *p;
	}

	return pWrite;
}

//---------------------------------------------------------------------------
//
/// Convert the Human68k side name
///
/// Once more, execute SetHost().
/// Carry out name conversion to the 18+3 standard.
/// Automatically delete spaces in the beginning and end of the names, since Human68k can't handle them.
/// The directory entry name segment is created at the time of conversion using knowledge of the location of the file name extension.
/// Afterwards, a file name validity check is performed. (Ex. file names consisting of 8 spaces only.)
/// No file name duplication check is performed so be careful. Such validation is carried out in classes higher up.
/// Adhers to the naming standards of: TwentyOne version 1.36c modified +14 patchlevel9 or later
//
//---------------------------------------------------------------------------
void CHostFilename::ConvertHuman(int nCount)
{
	// Don't do conversion for special directory names
	if (m_szHost[0] == '.' &&
		(m_szHost[1] == '\0' || (m_szHost[1] == '.' && m_szHost[2] == '\0'))) {
		strcpy((char*)m_szHuman, m_szHost);

		m_bCorrect = true;
		m_pszHumanLast = m_szHuman + strlen((const char*)m_szHuman);
		m_pszHumanExt = m_pszHumanLast;
		return;
	}

	size_t nMax = 18;	// Number of bytes for the base segment (base name and extension)
	uint32_t nOption = CFileSys::GetFileOption();
	if (nOption & WINDRV_OPT_CONVERT_LENGTH)
		nMax = 8;

	// Preparations to adjust the base name segment
	BYTE szNumber[8];
	BYTE* pNumber = nullptr;
	if (nCount >= 0) {
		pNumber = &szNumber[8];
		for (uint32_t i = 0; i < 5; i++) {	// Max 5+1 digits (always leave the first 2 bytes of the base name)
			int n = nCount % 36;
			nMax--;
			pNumber--;
			*pNumber = (BYTE)(n + (n < 10 ? '0' : 'A' - 10));
			nCount /= 36;
			if (nCount == 0)
				break;
		}
		nMax--;
		pNumber--;
		auto c = (BYTE)((nOption >> 24) & 0x7F);
		if (c == 0)
			c = XM6_HOST_FILENAME_MARK;
		*pNumber = c;
	}

	// Char conversion
	BYTE szHuman[FILEPATH_MAX];
	const BYTE* pFirst = szHuman;
	BYTE* pLast;
	BYTE* pExt = nullptr;

	{
		char szHost[FILEPATH_MAX];
		strcpy(szHost, m_szHost);
		auto pRead = (const BYTE*)szHost;
		BYTE* pWrite = szHuman;
		const auto pPeriod = SeparateExt(pRead);

		for (bool bFirst = true;; bFirst = false) {
			BYTE c = *pRead++;
			switch (c) {
				case ' ':
					if (nOption & WINDRV_OPT_REDUCED_SPACE)
						continue;
					if (nOption & WINDRV_OPT_CONVERT_SPACE)
						c = '_';
					else if (pWrite == szHuman)
						continue;	// Ignore spaces in the beginning
					break;
				case '=':
				case '+':
					if (nOption & WINDRV_OPT_REDUCED_BADCHAR)
						continue;
					if (nOption & WINDRV_OPT_CONVERT_BADCHAR)
						c = '_';
					break;
				case '-':
					if (bFirst) {
						if (nOption & WINDRV_OPT_REDUCED_HYPHEN)
							continue;
						if (nOption & WINDRV_OPT_CONVERT_HYPHEN)
							c = '_';
						break;
					}
					if (nOption & WINDRV_OPT_REDUCED_HYPHENS)
						continue;
					if (nOption & WINDRV_OPT_CONVERT_HYPHENS)
						c = '_';
					break;
				case '.':
					if (pRead - 1 == pPeriod) {		// Make exception for Human68k extensions
						pExt = pWrite;
						break;
					}
					if (bFirst) {
						if (nOption & WINDRV_OPT_REDUCED_PERIOD)
							continue;
						if (nOption & WINDRV_OPT_CONVERT_PERIOD)
							c = '_';
						break;
					}
					if (nOption & WINDRV_OPT_REDUCED_PERIODS)
						continue;
					if (nOption & WINDRV_OPT_CONVERT_PERIODS)
						c = '_';
					break;
				default:
					break;
			}
			*pWrite++ = c;
			if (c == '\0')
				break;
		}

		pLast = pWrite - 1;
	}

	// Adjust extensions
	if (pExt) {
		// Delete spaces at the end
		while (pExt < pLast - 1 && *(pLast - 1) == ' ') {
			pLast--;
			auto p = pLast;
			*p = '\0';
		}

		// Delete if the file name disappeared after conversion
		if (pExt + 1 >= pLast) {
			pLast = pExt;
			*pLast = '\0';		// Just in case
		}
	} else {
		pExt = pLast;
	}

	// Introducing the cast of characters
	//
	// pFirst: I'm the glorious leader. The start of the file name.
	// pCut: A.k.a. Phase. Location of the initial period. Afterwards becomes the end of the base name.
	// pSecond: Hello there! I'm the incredible Murdock. The start of the file name extension. What's it to you?
	// pExt: B.A. Baracus. The Human68k extension genius. But don't you dare giving me more than 3 chars, fool.
	// The location of the final period. If not applicable, gets the same value as pLast.
	//
	// ↓pFirst            ↓pStop ↓pSecond ←                ↓pExt
	// T h i s _ i s _ a . V e r y . L o n g . F i l e n a m e . t x t \0
	//         ↑pCut ← ↑pCut initial location                   ↑pLast
	//
	// The above example becomes "This.Long.Filename.txt" after conversion

	// Evaluate first char
	const BYTE* pCut = pFirst;
	const BYTE* pStop = pExt - nMax;	// Allow for up to 17 bytes for extension (leave base name)
	if (pFirst < pExt) {
		pCut++;		// 1 byte always uses the base name
		BYTE c = *pFirst;
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// Specifically 0x81~0x9F 0xE0~0xEF
			pCut++;		// Base name. At least 2 bytes.
			pStop++;	// File extension. Max 16 bytes.
		}
	}
	if (pStop < pFirst)
		pStop = pFirst;

	// Evaluate base name
	pCut = (const BYTE*)strchr((const char*)pCut, '.');	// The 2nd byte of Shift-JIS is always 0x40 or higher, so this is ok
	if (pCut == nullptr)
		pCut = pLast;
	if ((size_t)(pCut - pFirst) > nMax)
		pCut = pFirst + nMax;	// Execute Shift-JIS 2 byte evaluation/adjustment later. Not allowed to do it here.

	// Evaluate extension
	const BYTE* pSecond = pExt;
	const BYTE* p;
	for (p = pExt - 1; pStop < p; p--) {
		if (*p == '.')
			pSecond = p;	// The 2nd byte of Shift-JIS is always 0x40 or higher, so this is ok
	}

	// Shorten base name
	// Length of extension segment
	if (size_t nExt = pExt - pSecond; (size_t)(pCut - pFirst) + nExt > nMax)
		pCut = pFirst + nMax - nExt;
	// If in the middle of a 2 byte char, shorten even further
	for (p = pFirst; p < pCut; p++) {
		BYTE c = *p;
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// Specifically 0x81~0x9F 0xE0~0xEF
			p++;
			if (p >= pCut) {
				pCut--;
				break;
			}
		}
	}

	// Joining the name
	BYTE* pWrite = m_szHuman;
	pWrite = CopyName(pWrite, pFirst, pCut);	// Transfer the base name
	if (pNumber)
		pWrite = CopyName(pWrite, pNumber, &szNumber[8]);	// Transfer the adjustment char
	pWrite = CopyName(pWrite, pSecond, pExt);	// Transfer the extension name
	m_pszHumanExt = pWrite;						// Store the extention position
	pWrite = CopyName(pWrite, pExt, pLast);		// Transfer the Human68k extension
	m_pszHumanLast = pWrite;					// Store the end position
	*pWrite = '\0';

	// Confirm the conversion results
	m_bCorrect = true;

	// Fail if the base file name does not exist
	if (m_pszHumanExt <= m_szHuman)
		m_bCorrect = false;

	// Fail if the base file name is more than 1 char and ends with a space
	// While it is theoretically valid to have a base file name exceed 8 chars,
	// Human68k is unable to handle it, so failing this case too.
	else if (m_pszHumanExt[-1] == ' ')
		m_bCorrect = false;

	// Fail if the conversion result is the same as a special directory name
	if (m_szHuman[0] == '.' &&
		(m_szHuman[1] == '\0' || (m_szHuman[1] == '.' && m_szHuman[2] == '\0')))
		m_bCorrect = false;
}

//---------------------------------------------------------------------------
//
/// Human68k side name duplication
///
/// Duplicates the file name segment data, then executes the correspoding initialization with ConvertHuman().
//
//---------------------------------------------------------------------------
void CHostFilename::CopyHuman(const BYTE* szHuman)
{
	assert(szHuman);
	assert(strlen((const char*)szHuman) < 23);

	strcpy((char*)m_szHuman, (const char*)szHuman);
	m_bCorrect = true;
	m_pszHumanLast = m_szHuman + strlen((const char*)m_szHuman);
	m_pszHumanExt = SeparateExt(m_szHuman);
}

//---------------------------------------------------------------------------
//
/// Set Human68k directory entry
///
/// Apply the set file name to the directory entry with ConvertHuman().
//
//---------------------------------------------------------------------------
void CHostFilename::SetEntryName()
{
	// Set file name
	const BYTE* p = m_szHuman;
	size_t i;
	for (i = 0; i < 8; i++) {
		if (p < m_pszHumanExt)
			m_dirHuman.name[i] = *p++;
		else
			m_dirHuman.name[i] = ' ';
	}

	for (i = 0; i < 10; i++) {
		if (p < m_pszHumanExt)
			m_dirHuman.add[i] = *p++;
		else
			m_dirHuman.add[i] = '\0';
	}

	if (*p == '.')
		p++;
	for (i = 0; i < 3; i++) {
		BYTE c = *p;
		if (c)
			p++;
		m_dirHuman.ext[i] = c;
	}
}

//---------------------------------------------------------------------------
//
/// Investigate if the Human68k side name has been processed
//
//---------------------------------------------------------------------------
bool CHostFilename::isReduce() const
{
	return strcmp((const char *)m_szHost, (const char*)m_szHuman) != 0;
}

//---------------------------------------------------------------------------
//
/// Evaluate Human68k directory entry attribute
//
//---------------------------------------------------------------------------
int CHostFilename::CheckAttribute(uint32_t nHumanAttribute) const
{
	BYTE nAttribute = m_dirHuman.attr;
	if ((nAttribute & (Human68k::AT_ARCHIVE | Human68k::AT_DIRECTORY | Human68k::AT_VOLUME)) == 0)
		nAttribute |= Human68k::AT_ARCHIVE;

	return nAttribute & nHumanAttribute;
}

//---------------------------------------------------------------------------
//
/// Split the extension from Human68k file name
//
//---------------------------------------------------------------------------
const BYTE* CHostFilename::SeparateExt(const BYTE* szHuman)		// static
{
	// Obtain the file name length
	size_t nLength = strlen((const char*)szHuman);
	const BYTE* pFirst = szHuman;
	const BYTE* pLast = pFirst + nLength;

	// Confirm the position of the Human68k extension
	auto pExt = (const BYTE*)strrchr((const char*)pFirst, '.');	// The 2nd byte of Shift-JIS is always 0x40 or higher, so this is ok
	if (pExt == nullptr)
		pExt = pLast;
	// Special handling of the pattern where the file name is 20~22 chars, and the 19th char is '.' or ends with '.'
	if (20 <= nLength && nLength <= 22 && pFirst[18] == '.' && pFirst[nLength - 1] == '.')
		pExt = pFirst + 18;
	// Calculate the number of chars in the extension (-1:None 0:Only period 1~3:Human68k extension 4 or above:extension name)
	// Consider it an extension only when '.' is anywhere except the beginning of the string, and between 1~3 chars long
	if (size_t nExt = pLast - pExt - 1; pExt == pFirst || nExt < 1 || nExt > 3)
		pExt = pLast;

	return pExt;
}

//===========================================================================
//
//	Directory entry: path name
//
//===========================================================================

uint32_t CHostPath::g_nId;				///< Identifier creation counter

CHostPath::~CHostPath()
{
	Clean();
}

//---------------------------------------------------------------------------
//
/// File name memory allocation
///
/// In most cases, the length of the host side file name is way shorter
/// than the size of the buffer. In addition, file names may be created in huge volumes.
/// Therefore, allocate variable lengths that correspond to the number of chars.
//
//---------------------------------------------------------------------------
CHostPath::ring_t* CHostPath::Alloc(size_t nLength)	// static
{
	assert(nLength < FILEPATH_MAX);

	size_t n = offsetof(ring_t, f) + CHostFilename::Offset() + (nLength + 1) * sizeof(TCHAR);
	auto p = (ring_t*)malloc(n);
	assert(p);

	p->r.Init();	// This is nothing to worry about!

	return p;
}

//---------------------------------------------------------------------------
//
// Release file name allocations
//
//---------------------------------------------------------------------------
void CHostPath::Free(ring_t* pRing)	// static
{
	assert(pRing);

	pRing->~ring_t();
	free(pRing);
}

//---------------------------------------------------------------------------
//
/// Initialize for reuse
//
//---------------------------------------------------------------------------
void CHostPath::Clean()
{
	Release();

	// Release all file names
	ring_t* p;
	while ((p = (ring_t*)m_cRing.Next()) != (ring_t*)&m_cRing) {
		Free(p);
	}
}

//---------------------------------------------------------------------------
//
/// Specify Human68k side names directly
//
//---------------------------------------------------------------------------
void CHostPath::SetHuman(const BYTE* szHuman)
{
	assert(szHuman);
	assert(strlen((const char*)szHuman) < HUMAN68K_PATH_MAX);

	strcpy((char*)m_szHuman, (const char*)szHuman);
}

//---------------------------------------------------------------------------
//
/// Specify host side names directly
//
//---------------------------------------------------------------------------
void CHostPath::SetHost(const TCHAR* szHost)
{
	assert(szHost);
	assert(strlen(szHost) < FILEPATH_MAX);

	strcpy(m_szHost, szHost);
}

//---------------------------------------------------------------------------
//
/// Compare arrays (supports wildcards)
//
//---------------------------------------------------------------------------
int CHostPath::Compare(const BYTE* pFirst, const BYTE* pLast, const BYTE* pBufFirst, const BYTE* pBufLast)
{
	assert(pFirst);
	assert(pLast);
	assert(pBufFirst);
	assert(pBufLast);

	// Compare chars
	bool bSkip0 = false;
	bool bSkip1 = false;
	for (const BYTE* p = pFirst; p < pLast; p++) {
		// Read 1 char
		BYTE c = *p;
		BYTE d = '\0';
		if (pBufFirst < pBufLast)
			d = *pBufFirst++;

		// Ajust char for comparison
		if (!bSkip0) {
			if (!bSkip1) {	// First byte for both c and d
				if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// Specifically 0x81~0x9F 0xE0~0xEF
					bSkip0 = true;
				}
				if ((0x80 <= d && d <= 0x9F) || 0xE0 <= d) {	// Specifically 0x81~0x9F 0xE0~0xEF
					bSkip1 = true;
				}
				if (c == d)
					continue;	// Finishes the evaluation here with high probability
				if ((CFileSys::GetFileOption() & WINDRV_OPT_ALPHABET) == 0) {
					if ('A' <= c && c <= 'Z')
						c += 'a' - 'A';	// To lower case
					if ('A' <= d && d <= 'Z')
						d += 'a' - 'A';	// To lower case
				}
				// Unify slashes and backslashes for comparison
				if (c == '\\') {
					c = '/';
				}
				if (d == '\\') {
					d = '/';
				}
			} else {		// Only c is first byte
				if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// Specifically 0x81~0x9F 0xE0~0xEF
					bSkip0 = true;
				}
				bSkip1 = false;
			}
		} else {
			if (!bSkip1) {	// Only d is first byte
				bSkip0 = false;
				if ((0x80 <= d && d <= 0x9F) || 0xE0 <= d) {	// Specifically 0x81~0x9F 0xE0~0xEF
					bSkip1 = true;
				}
			} else {		// Second byte for both c and d
				bSkip0 = false;
				bSkip1 = false;
			}
		}

		// Compare
		if (c == d)
			continue;
		if (c == '?')
			continue;
		return 1;
	}
	if (pBufFirst < pBufLast)
		return 2;

	return 0;
}

//---------------------------------------------------------------------------
//
/// Compare Human68k side name
//
//---------------------------------------------------------------------------
bool CHostPath::isSameHuman(const BYTE* szHuman) const
{
	assert(szHuman);

	// Calulate number of chars
	size_t nLength = strlen((const char*)m_szHuman);
	size_t n = strlen((const char*)szHuman);

	// Check number of chars
	if (nLength != n)
		return false;

	// Compare Human68k path name
	return Compare(m_szHuman, m_szHuman + nLength, szHuman, szHuman + n) == 0;
}

bool CHostPath::isSameChild(const BYTE* szHuman) const
{
	assert(szHuman);

	// Calulate number of chars
	size_t nLength = strlen((const char*)m_szHuman);
	size_t n = strlen((const char*)szHuman);

	// Check number of chars
	if (nLength < n)
		return false;

	// Compare Human68k path name
	return Compare(m_szHuman, m_szHuman + n, szHuman, szHuman + n) == 0;
}

//---------------------------------------------------------------------------
//
/// Find file name
///
/// Check if whether it is a perfect match with the cache buffer, and return the name if found.
/// Path names are excempted.
/// Make sure to lock from the top.
//
//---------------------------------------------------------------------------
const CHostFilename* CHostPath::FindFilename(const BYTE* szHuman, uint32_t nHumanAttribute) const
{
	assert(szHuman);

	// Calulate number of chars
	const BYTE* pFirst = szHuman;
	size_t nLength = strlen((const char*)pFirst);
	const BYTE* pLast = pFirst + nLength;

	// Find something that matches perfectly with either of the stored file names
	const ring_t* p = (ring_t*)m_cRing.Next();
	for (; p != (const ring_t*)&m_cRing; p = (ring_t*)p->r.Next()) {
		if (p->f.CheckAttribute(nHumanAttribute) == 0)
			continue;
		// Calulate number of chars
		const BYTE* pBufFirst = p->f.GetHuman();
		const BYTE* pBufLast = p->f.GetHumanLast();
		// Check number of chars
		if (size_t nBufLength = pBufLast - pBufFirst; nLength != nBufLength)
			continue;
		// File name check
		if (Compare(pFirst, pLast, pBufFirst, pBufLast) == 0)
			return &p->f;
	}

	return nullptr;
}

//---------------------------------------------------------------------------
//
/// Find file name (with wildcard support)
///
/// Check if whether it is a perfect match with the cache buffer, and return the name if found.
/// Path names are excempted.
/// Make sure to lock from the top.
//
//---------------------------------------------------------------------------
const CHostFilename* CHostPath::FindFilenameWildcard(const BYTE* szHuman, uint32_t nHumanAttribute, find_t* pFind) const
{
	assert(szHuman);
	assert(pFind);

	// Split the base file name and Human68k file extension
	const BYTE* pFirst = szHuman;
	const BYTE* pLast = pFirst + strlen((const char*)pFirst);
	const BYTE* pExt = CHostFilename::SeparateExt(pFirst);

	// Move to the start position
	auto p = (const ring_t*)m_cRing.Next();
	if (pFind->count > 0) {
		if (pFind->id == m_nId) {
			// If the same directory entry, continue right away from the previous position
			p = pFind->pos;
		} else {
			// Find the start position in the directory entry contents
			uint32_t n = 0;
			for (;; p = (const ring_t*)p->r.Next()) {
				if (p == (const ring_t*)&m_cRing) {
					// Extrapolate from the count when the same entry isn't found (just in case)
					p = (const ring_t*)m_cRing.Next();
					n = 0;
					for (; p != (const ring_t*)&m_cRing; p = (const ring_t*)p->r.Next()) {
						if (n >= pFind->count)
							break;
						n++;
					}
					break;
				}
				if (p->f.isSameEntry(&pFind->entry)) {
					// Same entry is found
					pFind->count = n;
					break;
				}
				n++;
			}
		}
	}

	// Find files
	for (; p != (const ring_t*)&m_cRing; p = (const ring_t*)p->r.Next()) {
		pFind->count++;

		if (p->f.CheckAttribute(nHumanAttribute) == 0)
			continue;

		// Split the base file name and Human68k file extension
		const BYTE* pBufFirst = p->f.GetHuman();
		const BYTE* pBufLast = p->f.GetHumanLast();
		const BYTE* pBufExt = p->f.GetHumanExt();

		// Compare base file name
		if (Compare(pFirst, pExt, pBufFirst, pBufExt))
			continue;

		// Compare Human68k extension
		// In the case of a '.???' extension, match the Human68k extension without period.
		if (strcmp((const char*)pExt, ".???") == 0 ||
			Compare(pExt, pLast, pBufExt, pBufLast) == 0) {
			// Store the contents of the next candidate's directory entry
			const auto pNext = (const ring_t*)p->r.Next();
			pFind->id = m_nId;
			pFind->pos = pNext;
			if (pNext != (const ring_t*)&m_cRing)
				memcpy(&pFind->entry, pNext->f.GetEntry(), sizeof(pFind->entry));
			else
				memset(&pFind->entry, 0, sizeof(pFind->entry));
			return &p->f;
		}
	}

	pFind->id = m_nId;
	pFind->pos = p;
	memset(&pFind->entry, 0, sizeof(pFind->entry));
	return nullptr;
}

//---------------------------------------------------------------------------
//
/// Confirm that the file update has been carried out
//
//---------------------------------------------------------------------------
bool CHostPath::isRefresh() const
{
	return m_bRefresh;
}

//---------------------------------------------------------------------------
//
/// ASCII sort function
//
//---------------------------------------------------------------------------
int AsciiSort(const dirent **a, const dirent **b)
{
	return strcmp((*a)->d_name, (*b)->d_name);
}

//---------------------------------------------------------------------------
//
/// Reconstruct the file
///
/// Here we carry out the first host side file system observation.
/// Always lock from the top.
//
//---------------------------------------------------------------------------
void CHostPath::Refresh()
{
	assert(strlen(m_szHost) + 22 < FILEPATH_MAX);

	// Store time stamp
	Backup();

	TCHAR szPath[FILEPATH_MAX];
	strcpy(szPath, m_szHost);

	// Update refresh flag
	m_bRefresh = false;

	// Store previous cache contents
	CRing cRingBackup;
	m_cRing.InsertRing(&cRingBackup);

	// Register file name
	bool bUpdate = false;
	dirent **pd = nullptr;
	int nument = 0;
	int maxent = XM6_HOST_DIRENTRY_FILE_MAX;
	for (int i = 0; i < maxent; i++) {
		TCHAR szFilename[FILEPATH_MAX];
		if (pd == nullptr) {
			nument = scandir(S2U(szPath), &pd, nullptr, AsciiSort);
			if (nument == -1) {
				pd = nullptr;
				break;
			}
			maxent = nument;
		}

		// When at the top level directory, exclude current and parent
		const dirent* pe = pd[i];
		if (m_szHuman[0] == '/' && m_szHuman[1] == 0) {
			if (strcmp(pe->d_name, ".") == 0 || strcmp(pe->d_name, "..") == 0) {
				continue;
			}
		}

		// Get file name
		strcpy(szFilename, U2S(pe->d_name));

		// Allocate file name memory
		ring_t* pRing = Alloc(strlen(szFilename));
		CHostFilename* pFilename = &pRing->f;
		pFilename->SetHost(szFilename);

		// If there is a relevant file name in the previous cache, prioritize that for the Human68k name
		auto pCache = (ring_t*)cRingBackup.Next();
		for (;;) {
			if (pCache == (ring_t*)&cRingBackup) {
				pCache = nullptr;			// No relevant entry
				bUpdate = true;			// Confirm new entry
				pFilename->ConvertHuman();
				break;
			}
			if (strcmp(pFilename->GetHost(), pCache->f.GetHost()) == 0) {
				pFilename->CopyHuman(pCache->f.GetHuman());	// Copy Human68k name
				break;
			}
			pCache = (ring_t*)pCache->r.Next();
		}

		// If there is a new entry, carry out file name duplication check.
		// If the host side file name changed, or if Human68k cannot express the file name,
		// generate a new file name that passes all the below checks:
		// - File name correctness
		// - No duplicated names in previous entries
		// - No entity with the same name exists
		if (pFilename->isReduce() || !pFilename->isCorrect()) {	// Confirm that file name update is required
			for (uint32_t n = 0; n < XM6_HOST_FILENAME_PATTERN_MAX; n++) {
				// Confirm file name validity
				if (pFilename->isCorrect()) {
					// Confirm match with previous entry
					const CHostFilename* pCheck = FindFilename(pFilename->GetHuman());
					if (pCheck == nullptr) {
						// If no match, confirm existence of real file
						strcpy(szPath, m_szHost);
						strcat(szPath, (const char*)pFilename->GetHuman());
						if (struct stat sb; stat(S2U(szPath), &sb))
							break;	// Discover available patterns
					}
				}
				// Generate new name
				pFilename->ConvertHuman(n);
			}
		}

		// Directory entry name
		pFilename->SetEntryName();

		// Get data
		strcpy(szPath, m_szHost);
		strcat(szPath, U2S(pe->d_name));

		struct stat sb;
		if (stat(S2U(szPath), &sb))
			continue;

		BYTE nHumanAttribute = Human68k::AT_ARCHIVE;
		if (S_ISDIR(sb.st_mode))
			nHumanAttribute = Human68k::AT_DIRECTORY;
		if ((sb.st_mode & 0200) == 0)
			nHumanAttribute |= Human68k::AT_READONLY;
		pFilename->SetEntryAttribute(nHumanAttribute);

		auto nHumanSize = (uint32_t)sb.st_size;
		pFilename->SetEntrySize(nHumanSize);

		uint16_t nHumanDate = 0;
		uint16_t nHumanTime = 0;
		if (tm pt = {}; localtime_r(&sb.st_mtime, &pt) != nullptr) {
			nHumanDate = (uint16_t)(((pt.tm_year - 80) << 9) | ((pt.tm_mon + 1) << 5) | pt.tm_mday);
			nHumanTime = (uint16_t)((pt.tm_hour << 11) | (pt.tm_min << 5) | (pt.tm_sec >> 1));
		}
		pFilename->SetEntryDate(nHumanDate);
		pFilename->SetEntryTime(nHumanTime);

		pFilename->SetEntryCluster(0);

		// Compare with previous cached contents
		if (pCache) {
			if (pCache->f.isSameEntry(pFilename->GetEntry())) {
				Free(pRing);			// Destroy entry that was created here
				pRing = pCache;			// Use previous cache
			} else {
				Free(pCache);			// Remove from the next search target
				bUpdate = true;			// Flag for update if no match
			}
		}

		// Add to end of ring
		pRing->r.InsertTail(&m_cRing);
	}

	// Release directory entry
	if (pd) {
		for (int i = 0; i < nument; i++) {
			free(pd[i]);
		}
		free(pd);
	}

	// Delete remaining cache
	ring_t* p;
	while ((p = (ring_t*)cRingBackup.Next()) != (ring_t*)&cRingBackup) {
		bUpdate = true;					// Confirms the decrease in entries due to deletion
		Free(p);
	}

	// Update the identifier if the update has been carried out
	if (bUpdate)
		m_nId = ++g_nId;
}

//---------------------------------------------------------------------------
//
/// Store the host side time stamp
//
//---------------------------------------------------------------------------
void CHostPath::Backup()
{
	assert(m_szHost);
	assert(strlen(m_szHost) < FILEPATH_MAX);

	TCHAR szPath[FILEPATH_MAX];
	strcpy(szPath, m_szHost);
	size_t len = strlen(szPath);

	m_tBackup = 0;
	if (len > 1) {	// Don't do anything if it is the root directory
		len--;
		assert(szPath[len] == '/');
		szPath[len] = '\0';
		if (struct stat sb; stat(S2U(szPath), &sb) == 0)
			m_tBackup = sb.st_mtime;
	}
}

//---------------------------------------------------------------------------
//
/// Restore the host side time stamp
//
//---------------------------------------------------------------------------
void CHostPath::Restore() const
{
	assert(m_szHost);
	assert(strlen(m_szHost) < FILEPATH_MAX);

	TCHAR szPath[FILEPATH_MAX];
	strcpy(szPath, m_szHost);
	size_t len = strlen(szPath);

	if (m_tBackup) {
		assert(len);
		len--;
		assert(szPath[len] == '/');
		szPath[len] = '\0';

		utimbuf ut;
		ut.actime = m_tBackup;
		ut.modtime = m_tBackup;
		utime(szPath, &ut);
	}
}

//---------------------------------------------------------------------------
//
/// Update
//
//---------------------------------------------------------------------------
void CHostPath::Release()
{
	m_bRefresh = true;
}

//===========================================================================
//
//	Manage directory entries
//
//===========================================================================

CHostEntry::~CHostEntry()
{
	Clean();

#ifdef DEBUG
	// Confirm object
	for (const auto& d : m_pDrv) {
		assert(d == nullptr);
	}
#endif
}

//---------------------------------------------------------------------------
//
/// Initialize (when the driver is installed)
//
//---------------------------------------------------------------------------
void CHostEntry::Init() const
{
#ifdef DEBUG
	// Confirm object
	for (const auto& d : m_pDrv) {
		assert(d == nullptr);
	}
#endif
}

//---------------------------------------------------------------------------
//
/// Release (at startup and reset)
//
//---------------------------------------------------------------------------
void CHostEntry::Clean()
{
	// Delete object
	for (auto& d: m_pDrv) {
		delete d;
		d = nullptr;
	}
}

//---------------------------------------------------------------------------
//
/// Update all cache
//
//---------------------------------------------------------------------------
void CHostEntry::CleanCache() const
{
	for (const auto& d : m_pDrv) {
		if (d)
			d->CleanCache();
	}

	CHostPath::InitId();
}

//---------------------------------------------------------------------------
//
/// Update the cache for the specified unit
//
//---------------------------------------------------------------------------
void CHostEntry::CleanCache(uint32_t nUnit) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	m_pDrv[nUnit]->CleanCache();
}

//---------------------------------------------------------------------------
//
/// Update the cache for the specified path
//
//---------------------------------------------------------------------------
void CHostEntry::CleanCache(uint32_t nUnit, const BYTE* szHumanPath) const
{
	assert(szHumanPath);
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	m_pDrv[nUnit]->CleanCache(szHumanPath);
}

//---------------------------------------------------------------------------
//
/// Update all cache for the specified path and below
//
//---------------------------------------------------------------------------
void CHostEntry::CleanCacheChild(uint32_t nUnit, const BYTE* szHumanPath) const
{
	assert(szHumanPath);
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	m_pDrv[nUnit]->CleanCacheChild(szHumanPath);
}

//---------------------------------------------------------------------------
//
/// Delete cache for the specified path
//
//---------------------------------------------------------------------------
void CHostEntry::DeleteCache(uint32_t nUnit, const BYTE* szHumanPath) const
{
	assert(szHumanPath);
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	m_pDrv[nUnit]->DeleteCache(szHumanPath);
}

//---------------------------------------------------------------------------
//
/// Find host side names (path name + file name (can be abbreviated) + attribute)
//
//---------------------------------------------------------------------------
bool CHostEntry::Find(uint32_t nUnit, CHostFiles* pFiles) const
{
	assert(pFiles);
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->Find(pFiles);
}

//---------------------------------------------------------------------------
//
/// Drive settings
//
//---------------------------------------------------------------------------
void CHostEntry::SetDrv(uint32_t nUnit, CHostDrv* pDrv)
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit] == nullptr);

	m_pDrv[nUnit] = pDrv;
}

//---------------------------------------------------------------------------
//
/// Is it write-protected?
//
//---------------------------------------------------------------------------
bool CHostEntry::isWriteProtect(uint32_t nUnit) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->isWriteProtect();
}

//---------------------------------------------------------------------------
//
/// Is it accessible?
//
//---------------------------------------------------------------------------
bool CHostEntry::isEnable(uint32_t nUnit) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->isEnable();
}

//---------------------------------------------------------------------------
//
/// Media check
//
//---------------------------------------------------------------------------
bool CHostEntry::isMediaOffline(uint32_t nUnit) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->isMediaOffline();
}

//---------------------------------------------------------------------------
//
/// Get media byte
//
//---------------------------------------------------------------------------
BYTE CHostEntry::GetMediaByte(uint32_t nUnit) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetMediaByte();
}

//---------------------------------------------------------------------------
//
/// Get drive status
//
//---------------------------------------------------------------------------
uint32_t CHostEntry::GetStatus(uint32_t nUnit) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetStatus();
}

//---------------------------------------------------------------------------
//
/// Media change check
//
//---------------------------------------------------------------------------
bool CHostEntry::CheckMedia(uint32_t nUnit) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->CheckMedia();
}

//---------------------------------------------------------------------------
//
/// Eject
//
//---------------------------------------------------------------------------
void CHostEntry::Eject(uint32_t nUnit) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	m_pDrv[nUnit]->Eject();
}

//---------------------------------------------------------------------------
//
/// Get volume label
//
//---------------------------------------------------------------------------
void CHostEntry::GetVolume(uint32_t nUnit, TCHAR* szLabel) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	m_pDrv[nUnit]->GetVolume(szLabel);
}

//---------------------------------------------------------------------------
//
/// Get volume label from cache
//
//---------------------------------------------------------------------------
bool CHostEntry::GetVolumeCache(uint32_t nUnit, TCHAR* szLabel) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetVolumeCache(szLabel);
}

//---------------------------------------------------------------------------
//
/// Get capacity
//
//---------------------------------------------------------------------------
uint32_t CHostEntry::GetCapacity(uint32_t nUnit, Human68k::capacity_t* pCapacity) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetCapacity(pCapacity);
}

//---------------------------------------------------------------------------
//
/// Get cluster size from cache
//
//---------------------------------------------------------------------------
bool CHostEntry::GetCapacityCache(uint32_t nUnit, Human68k::capacity_t* pCapacity) const
{
	assert(nUnit < DRIVE_MAX);
	assert(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetCapacityCache(pCapacity);
}

//---------------------------------------------------------------------------
//
/// Split and copy the first element from the Human68k full path name
///
/// Get the first element from the Human68k full path name and delete the path separator char.
/// 23 bytes is required in the buffer to write to.
/// A Human68k path always starts with a '/'.
/// Throw an error if 2 '/' appears in sequence.
/// If the array ends with a '/' treat it as an empty array and don't trow an error.
//
//---------------------------------------------------------------------------
const BYTE* CHostDrv::SeparateCopyFilename(const BYTE* szHuman, BYTE* szBuffer)		// static
{
	assert(szHuman);
	assert(szBuffer);

	const size_t nMax = 22;
	const BYTE* p = szHuman;

	BYTE c = *p++;				// Read
	if (c != '/' && c != '\\')
		return nullptr;			// Error: Invalid path name

	// Insert one file
	size_t i = 0;
	for (;;) {
		c = *p;					// Read
		if (c == '\0')
			break;				// Exit if at the end of an array (return the end position)
		if (c == '/' || c == '\\') {
			if (i == 0)
				return nullptr;	// Error: Two separator chars appear in sequence
			break;				// Exit after reading the separator (return the char position)
		}
		p++;

		if (i >= nMax)
			return nullptr;		// Error: The first byte hits the end of the buffer
		szBuffer[i++] = c;		// Read

		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// Specifically 0x81~0x9F and 0xE0~0xEF
			c = *p++;			// Read
			if (c < 0x40)
				return nullptr;	// Error: Invalid Shift-JIS 2nd byte

			if (i >= nMax)
				return nullptr;	// Error: The second byte hits the end of the buffer
			szBuffer[i++] = c;	// Read
		}
	}
	szBuffer[i] = '\0';			// Read

	return p;
}

//---------------------------------------------------------------------------
//
/// Generate path and file name internally
//
//---------------------------------------------------------------------------
void CHostFiles::SetPath(const Human68k::namests_t* pNamests)
{
	assert(pNamests);

	pNamests->GetCopyPath(m_szHumanPath);
	pNamests->GetCopyFilename(m_szHumanFilename);
	m_nHumanWildcard = 0;
	m_nHumanAttribute = Human68k::AT_ARCHIVE;
	m_findNext.Clear();
}

//---------------------------------------------------------------------------
//
/// Find file on the Human68k side and create data on the host side
//
//---------------------------------------------------------------------------
bool CHostFiles::Find(uint32_t nUnit, const CHostEntry* pEntry)
{
	assert(pEntry);

	return pEntry->Find(nUnit, this);
}

//---------------------------------------------------------------------------
//
/// Find file name
//
//---------------------------------------------------------------------------
const CHostFilename* CHostFiles::Find(const CHostPath* pPath)
{
	assert(pPath);

	if (m_nHumanWildcard)
		return pPath->FindFilenameWildcard(m_szHumanFilename, m_nHumanAttribute, &m_findNext);

	return pPath->FindFilename(m_szHumanFilename, m_nHumanAttribute);
}

//---------------------------------------------------------------------------
//
/// Store the Human68k side search results
//
//---------------------------------------------------------------------------
void CHostFiles::SetEntry(const CHostFilename* pFilename)
{
	assert(pFilename);

	// Store Human68k directory entry
	memcpy(&m_dirHuman, pFilename->GetEntry(), sizeof(m_dirHuman));

	// Stire Human68k file name
	strcpy((char*)m_szHumanResult, (const char*)pFilename->GetHuman());
}

//---------------------------------------------------------------------------
//
/// Set host side name
//
//---------------------------------------------------------------------------
void CHostFiles::SetResult(const TCHAR* szPath)
{
	assert(szPath);
	assert(strlen(szPath) < FILEPATH_MAX);

	strcpy(m_szHostResult, szPath);
}

//---------------------------------------------------------------------------
//
/// Add file name to the host side name
//
//---------------------------------------------------------------------------
void CHostFiles::AddResult(const TCHAR* szPath)
{
	assert(szPath);
	assert(strlen(m_szHostResult) + strlen(szPath) < FILEPATH_MAX);

	strcat(m_szHostResult, szPath);
}

//---------------------------------------------------------------------------
//
/// Add a new Human68k file name to the host side name
//
//---------------------------------------------------------------------------
void CHostFiles::AddFilename()
{
	assert(strlen(m_szHostResult) + strlen((const char*)m_szHumanFilename) < FILEPATH_MAX);
	strncat(m_szHostResult, (const char*)m_szHumanFilename, ARRAY_SIZE(m_szHumanFilename));
}

//===========================================================================
//
//	File search memory manager
//
//===========================================================================

#ifdef _DEBUG
CHostFilesManager::~CHostFilesManager()
{
	// Confirm that the entity does not exist (just in case)
	assert(m_cRing.Next() == &m_cRing);
	assert(m_cRing.Prev() == &m_cRing);
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
/// Initialization (when the driver is installed)
//
//---------------------------------------------------------------------------
void CHostFilesManager::Init()
{

	// Confirm that the entity does not exist (just in case)
	assert(m_cRing.Next() == &m_cRing);
	assert(m_cRing.Prev() == &m_cRing);

	// Allocate memory
	for (uint32_t i = 0; i < XM6_HOST_FILES_MAX; i++) {
		auto p = new ring_t();
		p->r.Insert(&m_cRing);
	}
}

//---------------------------------------------------------------------------
//
/// Release (at startup and reset)
//
//---------------------------------------------------------------------------
void CHostFilesManager::Clean()
{

	// Release memory
	CRing* p;
	while ((p = m_cRing.Next()) != &m_cRing) {
		delete (ring_t*)p;
	}
}

CHostFiles* CHostFilesManager::Alloc(uint32_t nKey)
{
	assert(nKey);

	// Select from the end
	auto p = (ring_t*)m_cRing.Prev();

	// Move to the start of the ring
	p->r.Insert(&m_cRing);

	p->f.SetKey(nKey);

	return &p->f;
}

CHostFiles* CHostFilesManager::Search(uint32_t nKey)
{
	// assert(nKey);	// The search key may become 0 due to DPB damage

	// Find the relevant object
	auto p = (ring_t*)m_cRing.Next();
	for (; p != (ring_t*)&m_cRing; p = (ring_t*)p->r.Next()) {
		if (p->f.isSameKey(nKey)) {
			// Move to the start of the ring
			p->r.Insert(&m_cRing);
			return &p->f;
		}
	}

	return nullptr;
}

void CHostFilesManager::Free(CHostFiles* pFiles)
{
	assert(pFiles);

	// Release
	pFiles->SetKey(0);

	// Move to the end of the ring
	auto p = (ring_t*)((size_t)pFiles - offsetof(ring_t, f));
	p->r.InsertTail(&m_cRing);
}

//===========================================================================
//
//	FCB processing
//
//===========================================================================

//---------------------------------------------------------------------------
//
/// Set file open mode
//
//---------------------------------------------------------------------------
bool CHostFcb::SetMode(uint32_t nHumanMode)
{
	switch (nHumanMode & Human68k::OP_MASK) {
		case Human68k::OP_READ:
			m_pszMode = "rb";
			break;
		case Human68k::OP_WRITE:
			m_pszMode = "wb";
			break;
		case Human68k::OP_FULL:
			m_pszMode = "r+b";
			break;
		default:
			return false;
	}

	m_bFlag = (nHumanMode & Human68k::OP_SPECIAL) != 0;

	return true;
}

void CHostFcb::SetFilename(const TCHAR* szFilename)
{
	assert(szFilename);
	assert(strlen(szFilename) < FILEPATH_MAX);

	strcpy(m_szFilename, szFilename);
}

void CHostFcb::SetHumanPath(const BYTE* szHumanPath)
{
	assert(szHumanPath);
	assert(strlen((const char*)szHumanPath) < HUMAN68K_PATH_MAX);

	strcpy((char*)m_szHumanPath, (const char*)szHumanPath);
}

//---------------------------------------------------------------------------
//
/// Create file
///
/// Return false if error is thrown.
//
//---------------------------------------------------------------------------
bool CHostFcb::Create(uint32_t, bool bForce)
{
	assert((Human68k::AT_DIRECTORY | Human68k::AT_VOLUME) == 0);
	assert(strlen(m_szFilename) > 0);
	assert(m_pFile == nullptr);

	// Duplication check
	if (!bForce) {
		if (struct stat sb; stat(S2U(m_szFilename), &sb) == 0)
			return false;
	}

	// Create file
	m_pFile = fopen(S2U(m_szFilename), "w+b");	/// @warning The ideal operation is to overwrite each attribute

	return m_pFile != nullptr;
}

//---------------------------------------------------------------------------
//
/// File open
///
/// Return false if error is thrown.
//
//---------------------------------------------------------------------------
bool CHostFcb::Open()
{
	assert(strlen(m_szFilename) > 0);

	// Fail if directory
	if (struct stat st; stat(S2U(m_szFilename), &st) == 0 && ((st.st_mode & S_IFMT) == S_IFDIR)) {
		return false || m_bFlag;
	}

	// File open
	if (m_pFile == nullptr)
		m_pFile = fopen(S2U(m_szFilename), m_pszMode);

	return m_pFile != nullptr || m_bFlag;
}

//---------------------------------------------------------------------------
//
/// Read file
///
/// Handle a 0 byte read as normal operation too.
/// Return -1 if error is thrown.
//
//---------------------------------------------------------------------------
uint32_t CHostFcb::Read(BYTE* pBuffer, uint32_t nSize)
{
	assert(pBuffer);
	assert(m_pFile);

	size_t nResult = fread(pBuffer, sizeof(BYTE), nSize, m_pFile);
	if (ferror(m_pFile))
		nResult = (size_t)-1;

	return (uint32_t)nResult;
}

//---------------------------------------------------------------------------
//
/// Write file
///
/// Handle a 0 byte read as normal operation too.
/// Return -1 if error is thrown.
//
//---------------------------------------------------------------------------
uint32_t CHostFcb::Write(const BYTE* pBuffer, uint32_t nSize)
{
	assert(pBuffer);
	assert(m_pFile);

	size_t nResult = fwrite(pBuffer, sizeof(BYTE), nSize, m_pFile);
	if (ferror(m_pFile))
		nResult = (size_t)-1;

	return (uint32_t)nResult;
}

//---------------------------------------------------------------------------
//
/// Truncate file
///
/// Return false if error is thrown.
//
//---------------------------------------------------------------------------
bool CHostFcb::Truncate() const
{
	assert(m_pFile);

	return ftruncate(fileno(m_pFile), ftell(m_pFile)) == 0;
}

//---------------------------------------------------------------------------
//
/// File seek
///
/// Return -1 if error is thrown.
//
//---------------------------------------------------------------------------
uint32_t CHostFcb::Seek(uint32_t nOffset, Human68k::seek_t nHumanSeek)
{
	assert(m_pFile);

	int nSeek;
	switch (nHumanSeek) {
		case Human68k::seek_t::SK_BEGIN:
			nSeek = SEEK_SET;
			break;
		case Human68k::seek_t::SK_CURRENT:
			nSeek = SEEK_CUR;
			break;
			// case SK_END:
		default:
			nSeek = SEEK_END;
			break;
	}
	if (fseek(m_pFile, nOffset, nSeek))
		return (uint32_t)-1;

	return (uint32_t)ftell(m_pFile);
}

//---------------------------------------------------------------------------
//
/// Set file time stamp
///
/// Return false if error is thrown.
//
//---------------------------------------------------------------------------
bool CHostFcb::TimeStamp(uint32_t nHumanTime) const
{
	assert(m_pFile || m_bFlag);

	tm t = { };
	t.tm_year = (nHumanTime >> 25) + 80;
	t.tm_mon = ((nHumanTime >> 21) - 1) & 15;
	t.tm_mday = (nHumanTime >> 16) & 31;
	t.tm_hour = (nHumanTime >> 11) & 31;
	t.tm_min = (nHumanTime >> 5) & 63;
	t.tm_sec = (nHumanTime & 31) << 1;
	time_t ti = mktime(&t);
	if (ti == (time_t)-1)
		return false;
	utimbuf ut;
	ut.actime = ti;
	ut.modtime = ti;

	// This is for preventing the last updated time stamp to be overwritten upon closing.
	// Flush and synchronize before updating the time stamp.
	fflush(m_pFile);

	return utime(S2U(m_szFilename), &ut) == 0 || m_bFlag;
}

//---------------------------------------------------------------------------
//
/// File close
///
/// Return false if error is thrown.
//
//---------------------------------------------------------------------------
void CHostFcb::Close()
{
	// File close
	// Always initialize because of the Close→Free (internally one more Close) flow.
	if (m_pFile) {
		fclose(m_pFile);
		m_pFile = nullptr;
	}
}

//===========================================================================
//
// FCB processing manager
//
//===========================================================================

#ifdef _DEBUG
CHostFcbManager::~CHostFcbManager()
{
	// Confirm that the entity does not exist (just in case)
	assert(m_cRing.Next() == &m_cRing);
	assert(m_cRing.Prev() == &m_cRing);
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
// Initialization (when the driver is installed)
//
//---------------------------------------------------------------------------
void CHostFcbManager::Init()
{
	// Confirm that the entity does not exist (just in case)
	assert(m_cRing.Next() == &m_cRing);
	assert(m_cRing.Prev() == &m_cRing);

	// Memory allocation
	for (uint32_t i = 0; i < XM6_HOST_FCB_MAX; i++) {
		auto p = new ring_t;
		assert(p);
		p->r.Insert(&m_cRing);
	}
}

//---------------------------------------------------------------------------
//
// Clean (at startup/reset)
//
//---------------------------------------------------------------------------
void CHostFcbManager::Clean() const
{
	//  Fast task killer
	CRing* p;
	while ((p = m_cRing.Next()) != &m_cRing) {
		delete (ring_t*)p;
	}
}

CHostFcb* CHostFcbManager::Alloc(uint32_t nKey)
{
	assert(nKey);

	// Select from the end
	auto p = (ring_t*)m_cRing.Prev();

	// Error if in use (just in case)
	if (!p->f.isSameKey(0)) {
		assert(false);
		return nullptr;
	}

	// Move to the top of the ring
	p->r.Insert(&m_cRing);

	//  Set key
	p->f.SetKey(nKey);

	return &p->f;
}

CHostFcb* CHostFcbManager::Search(uint32_t nKey)
{
	assert(nKey);

	// Search for applicable objects
	auto p = (ring_t*)m_cRing.Next();
	while (p != (ring_t*)&m_cRing) {
		if (p->f.isSameKey(nKey)) {
			 // Move to the top of the ring
			p->r.Insert(&m_cRing);
			return &p->f;
		}
		p = (ring_t*)p->r.Next();
	}

	return nullptr;
}

void CHostFcbManager::Free(CHostFcb* pFcb)
{
	assert(pFcb);

	// Free
	pFcb->SetKey(0);
	pFcb->Close();

	// Move to the end of the ring
	auto p = (ring_t*)((size_t)pFcb - offsetof(ring_t, f));
	p->r.InsertTail(&m_cRing);
}

//===========================================================================
//
// Host side file system
//
//===========================================================================

uint32_t CFileSys::g_nOption;		// File name conversion flag

//---------------------------------------------------------------------------
//
/// Reset (close all)
//
//---------------------------------------------------------------------------
void CFileSys::Reset()
{
	// Initialize virtual sectors
	m_nHostSectorCount = 0;
	memset(m_nHostSectorBuffer, 0, sizeof(m_nHostSectorBuffer));

	// File search memory - release (on startup and reset)
	m_cFiles.Clean();

	// FCB operation memory (on startup and reset)
	m_cFcb.Clean();

	// Directory entry - release (on startup and reset)
	m_cEntry.Clean();

	// Initialize TwentyOne option monitoring
	m_nKernel = 0;
	m_nKernelSearch = 0;

	// Initialize operational flags
	SetOption(m_nOptionDefault);
}

//---------------------------------------------------------------------------
//
/// Initialize (device startup and load)
//
//---------------------------------------------------------------------------
void CFileSys::Init()
{
	// Initialize file search memory (device startup and load)
	m_cFiles.Init();

	// Initialize FCB operation memory (device startup and load)
	m_cFcb.Init();

	// Initialize directory entries (device startup and load)
	m_cEntry.Init();

	// Evaluate per-path setting validity
	uint32_t nDrives = m_nDrives;
	if (nDrives == 0) {
		// Use root directory instead of per-path settings
		strcpy(m_szBase[0], "/");
		m_nFlag[0] = 0;
		nDrives++;
	}

	// Register file system
	uint32_t nUnit = 0;
	for (uint32_t n = 0; n < nDrives; n++) {
		// Don't register is base path do not exist
		if (m_szBase[n][0] == '\0')
			continue;

		// Create 1 unit file system
		auto p = new CHostDrv;
		if (p) {
			m_cEntry.SetDrv(nUnit, p);
			p->Init(m_szBase[n], m_nFlag[n]);

			// To the next unit
			nUnit++;
		}
	}

	// Store the registered number of drives
	m_nUnits = nUnit;
}

//---------------------------------------------------------------------------
//
/// $40 - Device startup
//
//---------------------------------------------------------------------------
uint32_t CFileSys::InitDevice(const Human68k::argument_t* pArgument)
{
	InitOption(pArgument);

	// File system initialization
	Init();

	return m_nUnits;
}

//---------------------------------------------------------------------------
//
/// $41 - Directory check
//
//---------------------------------------------------------------------------
int CFileSys::CheckDir(uint32_t nUnit, const Human68k::namests_t* pNamests) const
{
	assert(pNamests);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// Avoid triggering a fatal error in mint when resuming with an invalid drive

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	if (f.isRootPath())
		return 0;
	f.SetPathOnly();
	if (!f.Find(nUnit, &m_cEntry))
		return FS_DIRNOTFND;

	return 0;
}

//---------------------------------------------------------------------------
//
/// $42 - Create directory
//
//---------------------------------------------------------------------------
int CFileSys::MakeDir(uint32_t nUnit, const Human68k::namests_t* pNamests) const
{
	assert(pNamests);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	assert(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Write-protect check
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetPathOnly();
	if (!f.Find(nUnit, &m_cEntry))
		return FS_INVALIDPATH;
	f.AddFilename();

	// Create directory
	if (mkdir(S2U(f.GetPath()), 0777))
		return FS_INVALIDPATH;

	// Update cache
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $43 - Delete directory
//
//---------------------------------------------------------------------------
int CFileSys::RemoveDir(uint32_t nUnit, const Human68k::namests_t* pNamests) const
{
	assert(pNamests);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	assert(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Write-protect check
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetAttribute(Human68k::AT_DIRECTORY);
	if (!f.Find(nUnit, &m_cEntry))
		return FS_DIRNOTFND;

	// Delete cache
	BYTE szHuman[HUMAN68K_PATH_MAX + 24];
	assert(strlen((const char*)f.GetHumanPath()) +
		strlen((const char*)f.GetHumanFilename()) < HUMAN68K_PATH_MAX + 24);
	strcpy((char*)szHuman, (const char*)f.GetHumanPath());
	strcat((char*)szHuman, (const char*)f.GetHumanFilename());
	strcat((char*)szHuman, "/");
	m_cEntry.DeleteCache(nUnit, szHuman);

	// Delete directory
	if (rmdir(S2U(f.GetPath())))
		return FS_CANTDELETE;

	// Update cache
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $44 - Change file name
//
//---------------------------------------------------------------------------
int CFileSys::Rename(uint32_t nUnit, const Human68k::namests_t* pNamests, const Human68k::namests_t* pNamestsNew) const
{
	assert(pNamests);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	assert(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Write-protect check
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (!f.Find(nUnit, &m_cEntry))
		return FS_FILENOTFND;

	CHostFiles fNew;
	fNew.SetPath(pNamestsNew);
	fNew.SetPathOnly();
	if (!fNew.Find(nUnit, &m_cEntry))
		return FS_INVALIDPATH;
	fNew.AddFilename();

	// Update cache
	if (f.GetAttribute() & Human68k::AT_DIRECTORY)
		m_cEntry.CleanCacheChild(nUnit, f.GetHumanPath());

	// Change file name
	char szFrom[FILENAME_MAX];
	char szTo[FILENAME_MAX];
	SJIS2UTF8(f.GetPath(), szFrom, FILENAME_MAX);
	SJIS2UTF8(fNew.GetPath(), szTo, FILENAME_MAX);
	if (rename(szFrom, szTo)) {
		return FS_FILENOTFND;
	}

	// Update cache
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());
	m_cEntry.CleanCache(nUnit, fNew.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $45 - Delete file
//
//---------------------------------------------------------------------------
int CFileSys::Delete(uint32_t nUnit, const Human68k::namests_t* pNamests) const
{
	assert(pNamests);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	assert(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Write-protect check
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	if (!f.Find(nUnit, &m_cEntry))
		return FS_FILENOTFND;

	// Delete file
	if (unlink(S2U(f.GetPath())))
		return FS_CANTDELETE;

	// Update cache
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $46 - Get/set file attribute
//
//---------------------------------------------------------------------------
int CFileSys::Attribute(uint32_t nUnit, const Human68k::namests_t* pNamests, uint32_t nHumanAttribute) const
{
	assert(pNamests);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// This occurs when resuming with an invalid drive

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (!f.Find(nUnit, &m_cEntry))
		return FS_FILENOTFND;

	// Exit if attribute is acquired
	if (nHumanAttribute == 0xFF)
		return f.GetAttribute();

	// Attribute check
	if (nHumanAttribute & Human68k::AT_VOLUME)
		return FS_CANTACCESS;

	// Write-protect check
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// Generate attribute
	if (uint32_t nAttribute = (nHumanAttribute & Human68k::AT_READONLY) | (f.GetAttribute() & ~Human68k::AT_READONLY);
		f.GetAttribute() != nAttribute) {
		struct stat sb;
		if (stat(S2U(f.GetPath()), &sb))
			return FS_FILENOTFND;
		mode_t m = sb.st_mode & 0777;
		if (nAttribute & Human68k::AT_READONLY)
			m &= 0555;	// ugo-w
		else
			m |= 0200;	// u+w

		// Set attribute
		if (chmod(S2U(f.GetPath()), m))
			return FS_FILENOTFND;
	}

	// Update cache
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	// Get attribute after changing
	if (!f.Find(nUnit, &m_cEntry))
		return FS_FILENOTFND;

	return f.GetAttribute();
}

//---------------------------------------------------------------------------
//
/// $47 - File search
//
//---------------------------------------------------------------------------
int CFileSys::Files(uint32_t nUnit, uint32_t nKey, const Human68k::namests_t* pNamests, Human68k::files_t* pFiles)
{
	assert(pNamests);
	assert(nKey);
	assert(pFiles);

	// Release if memory with the same key already exists
	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles != nullptr) {
		m_cFiles.Free(pHostFiles);
	}

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// This occurs when resuming with an invalid drive

	// Get volume label
	/** @note
	This skips the media check when getting the volume label to avoid triggering a fatal error
	with host side removable media (CD-ROM, etc.) Some poorly coded applications will attempt to
    get the volume label even though a proper error was thrown doing a media change check just before.
	*/
	if ((pFiles->fatr & (Human68k::AT_ARCHIVE | Human68k::AT_DIRECTORY | Human68k::AT_VOLUME))
		== Human68k::AT_VOLUME) {
		// Path check
		CHostFiles f;
		f.SetPath(pNamests);
		if (!f.isRootPath())
			return FS_FILENOTFND;

		// Immediately return the results without allocating buffer
		if (!FilesVolume(nUnit, pFiles))
			return FS_FILENOTFND;
		return 0;
	}

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Allocate buffer
	pHostFiles = m_cFiles.Alloc(nKey);
	if (pHostFiles == nullptr)
		return FS_OUTOFMEM;

	// Directory check
	pHostFiles->SetPath(pNamests);
	if (!pHostFiles->isRootPath()) {
		pHostFiles->SetPathOnly();
		if (!pHostFiles->Find(nUnit, &m_cEntry)) {
			m_cFiles.Free(pHostFiles);
			return FS_DIRNOTFND;
		}
	}

	// Enable wildcards
	pHostFiles->SetPathWildcard();
	pHostFiles->SetAttribute(pFiles->fatr);

	// Find file
	if (!pHostFiles->Find(nUnit, &m_cEntry)) {
		m_cFiles.Free(pHostFiles);
		return FS_FILENOTFND;
	}

	// Store search results
	pFiles->attr = (BYTE)pHostFiles->GetAttribute();
	pFiles->date = pHostFiles->GetDate();
	pFiles->time = pHostFiles->GetTime();
	pFiles->size = pHostFiles->GetSize();
	strcpy((char*)pFiles->full, (const char*)pHostFiles->GetHumanResult());

	// Specify pseudo-directory entry
	pFiles->sector = nKey;
	pFiles->offset = 0;

	return 0;
}

//---------------------------------------------------------------------------
//
/// $48 - Search next file
//
//---------------------------------------------------------------------------
int CFileSys::NFiles(uint32_t nUnit, uint32_t nKey, Human68k::files_t* pFiles)
{
	assert(nKey);
	assert(pFiles);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// This occurs when resuming with an invalid drive

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Find buffer
	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles == nullptr)
		return FS_INVALIDPTR;

	// Find file
	if (!pHostFiles->Find(nUnit, &m_cEntry)) {
		m_cFiles.Free(pHostFiles);
		return FS_FILENOTFND;
	}

	assert(pFiles->sector == nKey);
	assert(pFiles->offset == 0);

	// Store search results
	pFiles->attr = (BYTE)pHostFiles->GetAttribute();
	pFiles->date = pHostFiles->GetDate();
	pFiles->time = pHostFiles->GetTime();
	pFiles->size = pHostFiles->GetSize();
	strcpy((char*)pFiles->full, (const char*)pHostFiles->GetHumanResult());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $49 - Create new file
//
//---------------------------------------------------------------------------
int CFileSys::Create(uint32_t nUnit, uint32_t nKey, const Human68k::namests_t* pNamests, Human68k::fcb_t* pFcb, uint32_t nHumanAttribute, bool bForce)
{
	assert(pNamests);
	assert(nKey);
	assert(pFcb);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	assert(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Write-protect check
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// Release if memory with the same key already exists
	if (m_cFcb.Search(nKey) != nullptr)
		return FS_INVALIDPTR;

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetPathOnly();
	if (!f.Find(nUnit, &m_cEntry))
		return FS_INVALIDPATH;
	f.AddFilename();

	// Attribute check
	if (nHumanAttribute & (Human68k::AT_DIRECTORY | Human68k::AT_VOLUME))
		return FS_CANTACCESS;

	// Store path name
	CHostFcb* pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == nullptr)
		return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());
	pHostFcb->SetHumanPath(f.GetHumanPath());

	// Set open mode
	pFcb->mode = (uint16_t)((pFcb->mode & ~Human68k::OP_MASK) | Human68k::OP_FULL);
	if (!pHostFcb->SetMode(pFcb->mode)) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

	// Create file
	if (!pHostFcb->Create(nHumanAttribute, bForce)) {
		m_cFcb.Free(pHostFcb);
		return FS_FILEEXIST;
	}

	// Update cache
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $4A - File open
//
//---------------------------------------------------------------------------
int CFileSys::Open(uint32_t nUnit, uint32_t nKey, const Human68k::namests_t* pNamests, Human68k::fcb_t* pFcb)
{
	assert(pNamests);
	assert(nKey);
	assert(pFcb);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// This occurs when resuming with an invalid drive

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Write-protect check
	switch (pFcb->mode & Human68k::OP_MASK) {
		case Human68k::OP_WRITE:
		case Human68k::OP_FULL:
			if (m_cEntry.isWriteProtect(nUnit))
				return FS_FATAL_WRITEPROTECT;
			break;
		default: break;
	}

	// Release if memory with the same key already exists
	if (m_cFcb.Search(nKey) != nullptr)
		return FS_INVALIDPRM;

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (!f.Find(nUnit, &m_cEntry))
		return FS_FILENOTFND;

	// Time stamp
	pFcb->date = f.GetDate();
	pFcb->time = f.GetTime();

	// File size
	pFcb->size = f.GetSize();

	// Store path name
	CHostFcb* pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == nullptr)
		return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());
	pHostFcb->SetHumanPath(f.GetHumanPath());

	// Set open mode
	if (!pHostFcb->SetMode(pFcb->mode)) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

	// File open
	if (!pHostFcb->Open()) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPATH;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
/// $4B - File close
//
//---------------------------------------------------------------------------
int CFileSys::Close(uint32_t nUnit, uint32_t nKey, const Human68k::fcb_t* /* pFcb */)
{
	assert(nKey);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// This occurs when resuming with an invalid drive

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Throw error if memory with the same key does not exist
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == nullptr)
		return FS_INVALIDPRM;

	// File close and release memory
	m_cFcb.Free(pHostFcb);

	// Update cache
	if (pHostFcb->isUpdate())
		m_cEntry.CleanCache(nUnit, pHostFcb->GetHumanPath());

	/// TODO: Match the FCB status on close with other devices

	return 0;
}

//---------------------------------------------------------------------------
//
/// $4C - Read file
///
/// Clean exit when 0 bytes are read.
//
//---------------------------------------------------------------------------
int CFileSys::Read(uint32_t nKey, Human68k::fcb_t* pFcb, BYTE* pBuffer, uint32_t nSize)
{
	assert(nKey);
	assert(pFcb);
	// assert(pBuffer);			// Evaluate only when needed
	assert(nSize <= 0xFFFFFF);	// Clipped

	// Throw error if memory with the same key does not exist
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == nullptr)
		return FS_NOTOPENED;

	// Confirm the existence of the buffer
	if (pBuffer == nullptr) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDFUNC;
	}

	// Read
	uint32_t nResult;
	nResult = pHostFcb->Read(pBuffer, nSize);
	if (nResult == (uint32_t)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDFUNC;	// TODO: Should return error code 10 (read error) as well here
	}
	assert(nResult <= nSize);

	// Update file pointer
	pFcb->fileptr += nResult;	/// TODO: Maybe an overflow check is needed here?

	return nResult;
}

//---------------------------------------------------------------------------
//
/// $4D - Write file
///
/// Truncate file if 0 bytes are written.
//
//---------------------------------------------------------------------------
int CFileSys::Write(uint32_t nKey, Human68k::fcb_t* pFcb, const BYTE* pBuffer, uint32_t nSize)
{
	assert(nKey);
	assert(pFcb);
	// assert(pBuffer);			// Evaluate only when needed
	assert(nSize <= 0xFFFFFF);	// Clipped

	// Throw error if memory with the same key does not exist
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == nullptr)
		return FS_NOTOPENED;

	uint32_t nResult;
	if (nSize == 0) {
		// Truncate
		if (!pHostFcb->Truncate()) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTSEEK;
		}

		// Update file size
		pFcb->size = pFcb->fileptr;

		nResult = 0;
	} else {
		// Confirm the existence of the buffer
		if (pBuffer == nullptr) {
			m_cFcb.Free(pHostFcb);
			return FS_INVALIDFUNC;
		}

		// Write
		nResult = pHostFcb->Write(pBuffer, nSize);
		if (nResult == (uint32_t)-1) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTWRITE;	/// TODO: Should return error code 11 (write error) as well here
		}
		assert(nResult <= nSize);

		// Update file pointer
		pFcb->fileptr += nResult;	/// TODO: Do we need an overflow check here?

		// Update file size
		if (pFcb->size < pFcb->fileptr)
			pFcb->size = pFcb->fileptr;
	}

	// Update flag
	pHostFcb->SetUpdate();

	return nResult;
}

//---------------------------------------------------------------------------
//
/// $4E - File seek
//
//---------------------------------------------------------------------------
int CFileSys::Seek(uint32_t nKey, Human68k::fcb_t* pFcb, uint32_t nSeek, int nOffset)
{
	assert(pFcb);

	// Throw error if memory with the same key does not exist
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == nullptr)
		return FS_NOTOPENED;

	// Parameter check
	if (nSeek > (uint32_t)Human68k::seek_t::SK_END) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPRM;
	}

	// File seek
	uint32_t nResult = pHostFcb->Seek(nOffset, (Human68k::seek_t)nSeek);
	if (nResult == (uint32_t)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}

	// Update file pointer
	pFcb->fileptr = nResult;

	return nResult;
}

//---------------------------------------------------------------------------
//
/// $4F - Get/set file time stamp
///
/// Throw error when the top 16 bits are $FFFF.
//
//---------------------------------------------------------------------------
uint32_t CFileSys::TimeStamp(uint32_t nUnit, uint32_t nKey, Human68k::fcb_t* pFcb, uint32_t nHumanTime)
{
	assert(nKey);
	assert(pFcb);

	// Get only
	if (nHumanTime == 0)
		return ((uint32_t)pFcb->date << 16) | pFcb->time;

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	assert(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Write-protect check
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// Throw error if memory with the same key does not exist
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == nullptr)
		return FS_NOTOPENED;

	// Set time stamp
	if (!pHostFcb->TimeStamp(nHumanTime)) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPRM;
	}
	pFcb->date = (uint16_t)(nHumanTime >> 16);
	pFcb->time = (uint16_t)nHumanTime;

	// Update cache
	m_cEntry.CleanCache(nUnit, pHostFcb->GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $50 - Get capacity
//
//---------------------------------------------------------------------------
int CFileSys::GetCapacity(uint32_t nUnit, Human68k::capacity_t* pCapacity) const
{
	assert(pCapacity);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	assert(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Get capacity
	return m_cEntry.GetCapacity(nUnit, pCapacity);
}

//---------------------------------------------------------------------------
//
/// $51 - Inspect/control drive status
//
//---------------------------------------------------------------------------
int CFileSys::CtrlDrive(uint32_t nUnit, Human68k::ctrldrive_t* pCtrlDrive) const
{
	assert(pCtrlDrive);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// Avoid triggering a fatal error in mint when resuming with an invalid drive

	switch (pCtrlDrive->status) {
		case 0:		// Inspect status
		case 9:		// Inspect status 2
			pCtrlDrive->status = (BYTE)m_cEntry.GetStatus(nUnit);
			return pCtrlDrive->status;

		case 1:		// Eject
		case 2:		// Eject forbidden 1 (not implemented)
		case 3:		// Eject allowed 1 (not implemented)
		case 4:		// Flash LED when media is not inserted (not implemented)
		case 5:		// Turn off LED when media is not inserted (not implemented)
		case 6:		// Eject forbidden 2 (not implemented)
		case 7:		// Eject allowed 2 (not implemented)
			return 0;

		case 8:		// Eject inspection
			return 1;

		default:
			break;
	}

	return FS_INVALIDFUNC;
}

//---------------------------------------------------------------------------
//
/// $52 - Get DPB
///
/// If DPB cannot be acquired after resuming, the drive will be torn down internally in Human68k.
/// Therefore, treat even a unit out of bounds as normal operation.
//
//---------------------------------------------------------------------------
int CFileSys::GetDPB(uint32_t nUnit, Human68k::dpb_t* pDpb) const
{
	assert(pDpb);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;

	Human68k::capacity_t cap;
	BYTE media = Human68k::MEDIA_REMOTE;
	if (nUnit < m_nUnits) {
		media = m_cEntry.GetMediaByte(nUnit);

		// Acquire sector data
		if (!m_cEntry.GetCapacityCache(nUnit, &cap)) {
			// Carry out an extra media check here because it may be skipped when doing a manual eject
			if (!m_cEntry.isEnable(nUnit))
				goto none;

			// Media check
			if (m_cEntry.isMediaOffline(nUnit))
				goto none;

			// Get drive status
			m_cEntry.GetCapacity(nUnit, &cap);
		}
	} else {
	none:
		cap.clusters = 4;	// This is totally fine, right?
		cap.sectors = 64;
		cap.bytes = 512;
	}

	// Calculate number of shifts
	uint32_t nSize = 1;
	uint32_t nShift = 0;
	for (;;) {
		if (nSize >= cap.sectors)
			break;
		nSize <<= 1;
		nShift++;
	}

	// Sector number calculation
	//
	// In the following order:
	// Cluster 0: Unused
	// Cluster 1: FAT
	// Cluster 2: Root directory
	// Cluster 3: Data memory (pseudo-sector)
	uint32_t nFat = cap.sectors;
	uint32_t nRoot = cap.sectors * 2;
	uint32_t nData = cap.sectors * 3;

	// Set DPB
	pDpb->sector_size = cap.bytes;		// Bytes per sector
	pDpb->cluster_size =
		(BYTE)(cap.sectors - 1);				// Sectors per cluster - 1
	pDpb->shift = (BYTE)nShift;					// Number of cluster → sector shifts
	pDpb->fat_sector = (uint16_t)nFat;				// First FAT sector number
	pDpb->fat_max = 1;							// Number of FAT memory spaces
	pDpb->fat_size = (BYTE)cap.sectors;			// Number of sectors controlled by FAT (excluding copies)
	pDpb->file_max =
		(uint16_t)(cap.sectors * cap.bytes / 0x20);	// Number of files in the root directory
	pDpb->data_sector = (uint16_t)nData;			// First sector number of data memory
	pDpb->cluster_max = cap.clusters;		// Total number of clusters + 1
	pDpb->root_sector = (uint16_t)nRoot;			// First sector number of the root directory
	pDpb->media = media;						// Media byte

	return 0;
}

//---------------------------------------------------------------------------
//
/// $53 - Read sector
///
/// We use pseudo-sectors.
/// Buffer size is hard coded to $200 byte.
//
//---------------------------------------------------------------------------
int CFileSys::DiskRead(uint32_t nUnit, BYTE* pBuffer, uint32_t nSector, uint32_t nSize)
{
	assert(pBuffer);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// This occurs when resuming with an invalid drive

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Throw error if number of sectors exceed 1
	if (nSize != 1)
		return FS_INVALIDPRM;

	// Acquire sector data
	Human68k::capacity_t cap;
	if (!m_cEntry.GetCapacityCache(nUnit, &cap)) {
		// Get drive status
		m_cEntry.GetCapacity(nUnit, &cap);
	}

	// Access pseudo-directory entry
	const CHostFiles* pHostFiles = m_cFiles.Search(nSector);
	if (pHostFiles) {
		// Generate pseudo-directory entry
		auto dir = (Human68k::dirent_t*)pBuffer;
		memcpy(pBuffer, pHostFiles->GetEntry(), sizeof(*dir));
		memset(pBuffer + sizeof(*dir), 0xE5, 0x200 - sizeof(*dir));

		// Register the pseudo-sector number that points to the file entity inside the pseudo-directory.
		// Note that in lzdsys the sector number to read is calculated by the following formula:
		// (dirent.cluster - 2) * (dpb.cluster_size + 1) + dpb.data_sector
		/// @warning little endian only
		dir->cluster = (uint16_t)(m_nHostSectorCount + 2);		// Pseudo-sector number
		m_nHostSectorBuffer[m_nHostSectorCount] = nSector;	// Entity that points to the pseudo-sector
		m_nHostSectorCount++;
		m_nHostSectorCount %= XM6_HOST_PSEUDO_CLUSTER_MAX;

		return 0;
	}

	// Calculate the sector number from the cluster number
	uint32_t n = nSector - (3 * cap.sectors);
	uint32_t nMod = 1;
	if (cap.sectors) {
		// Beware that cap.sectors becomes 0 when media does not exist
		nMod = n % cap.sectors;
		n /= cap.sectors;
	}

	// Access the file entity
	if (nMod == 0 && n < XM6_HOST_PSEUDO_CLUSTER_MAX) {
		pHostFiles = m_cFiles.Search(m_nHostSectorBuffer[n]);	// Find entity
		if (pHostFiles) {
			// Generate pseudo-sector
			CHostFcb f;
			f.SetFilename(pHostFiles->GetPath());
			f.SetMode(Human68k::OP_READ);
			if (!f.Open())
				return FS_INVALIDPRM;
			memset(pBuffer, 0, 0x200);
			uint32_t nResult = f.Read(pBuffer, 0x200);
			f.Close();
			if (nResult == (uint32_t)-1)
				return FS_INVALIDPRM;

			return 0;
		}
	}

	return FS_INVALIDPRM;
}

//---------------------------------------------------------------------------
//
/// $54 - Write sector
//
//---------------------------------------------------------------------------
int CFileSys::DiskWrite(uint32_t nUnit) const
{
	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	assert(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Write-protect check
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// Thrust at reality
	return FS_INVALIDPRM;
}

//---------------------------------------------------------------------------
//
/// $55 - IOCTRL
//
//---------------------------------------------------------------------------
int CFileSys::Ioctrl(uint32_t nUnit, uint32_t nFunction, Human68k::ioctrl_t* pIoctrl)
{
	assert(pIoctrl);

	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// Avoid triggering a fatal error in mint when resuming with an invalid drive

	switch (nFunction) {
		case 0:
			// Acquire media byte
			pIoctrl->media = m_cEntry.GetMediaByte(nUnit);
			return 0;

		case 1:
			// Dummy for Human68k compatibility
			pIoctrl->param = -1;
			return 0;

		case 2:
			switch (pIoctrl->param) {
				case (uint32_t)-1:
					// Re-identify media
					m_cEntry.isMediaOffline(nUnit);
					return 0;

				case 0:
				case 1:
					// Dummy for Human68k compatibility
					return 0;

				default:
					return FS_NOTIOCTRL;
			}
			break;

		case (uint32_t)-1:
			// Resident evaluation
			memcpy(pIoctrl->buffer, "WindrvXM", 8);
			return 0;

		case (uint32_t)-2:
			// Set options
			SetOption(pIoctrl->param);
			return 0;

		case (uint32_t)-3:
			// Get options
			pIoctrl->param = GetOption();
			return 0;

		default:
			break;
	}

	return FS_NOTIOCTRL;
}

//---------------------------------------------------------------------------
//
/// $56 - Flush
//
//---------------------------------------------------------------------------
int CFileSys::Flush(uint32_t nUnit) const
{
	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// Avoid triggering a fatal error returning from a mint command when resuming with an invalid drive

	// Always succeed
	return 0;
}

//---------------------------------------------------------------------------
//
/// $57 - Media change check
//
//---------------------------------------------------------------------------
int CFileSys::CheckMedia(uint32_t nUnit) const
{
	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// Avoid triggering a fatal error in mint when resuming with an invalid drive

	// Media change check
	// Throw error when media is not inserted
	if (!m_cEntry.CheckMedia(nUnit)) {
		return FS_INVALIDFUNC;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
/// $58 - Lock
//
//---------------------------------------------------------------------------
int CFileSys::Lock(uint32_t nUnit) const
{
	// Unit check
	if (nUnit >= CHostEntry::DRIVE_MAX)
		return FS_FATAL_INVALIDUNIT;
	assert(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Always succeed
	return 0;
}

//---------------------------------------------------------------------------
//
/// Set options
//
//---------------------------------------------------------------------------
void CFileSys::SetOption(uint32_t nOption)
{
	// Clear cache when option settings change
	if (m_nOption ^ nOption)
		m_cEntry.CleanCache();

	m_nOption = nOption;
	g_nOption = nOption;
}

//---------------------------------------------------------------------------
//
/// Initialize options
//
//---------------------------------------------------------------------------
void CFileSys::InitOption(const Human68k::argument_t* pArgument)
{
	assert(pArgument);

	// Initialize number of drives
	m_nDrives = 0;

	const BYTE* pp = pArgument->buf;
	pp += strlen((const char*)pp) + 1;

	uint32_t nOption = m_nOptionDefault;
	for (;;) {
		assert(pp < pArgument->buf + sizeof(*pArgument));
		const BYTE* p = pp;
		BYTE c = *p++;
		if (c == '\0')
			break;

		uint32_t nMode;
		if (c == '+') {
			nMode = 1;
		} else if (c == '-') {
			nMode = 0;
		} else if (c == '/') {
			// Specify default base path
			if (m_nDrives < CHostEntry::DRIVE_MAX) {
				p--;
				strcpy(m_szBase[m_nDrives], (const char *)p);
				m_nDrives++;
			}
			pp += strlen((const char*)pp) + 1;
			continue;
		} else {
			// Continue since no option is specified
			pp += strlen((const char*)pp) + 1;
			continue;
		}

		for (;;) {
			c = *p++;
			if (c == '\0')
				break;

			uint32_t nBit = 0;
			switch (c) {
				case 'A': case 'a': nBit = WINDRV_OPT_CONVERT_LENGTH; break;
				case 'T': case 't': nBit = WINDRV_OPT_COMPARE_LENGTH; nMode ^= 1; break;
				case 'C': case 'c': nBit = WINDRV_OPT_ALPHABET; break;

				case 'E': nBit = WINDRV_OPT_CONVERT_PERIOD; break;
				case 'P': nBit = WINDRV_OPT_CONVERT_PERIODS; break;
				case 'N': nBit = WINDRV_OPT_CONVERT_HYPHEN; break;
				case 'H': nBit = WINDRV_OPT_CONVERT_HYPHENS; break;
				case 'X': nBit = WINDRV_OPT_CONVERT_BADCHAR; break;
				case 'S': nBit = WINDRV_OPT_CONVERT_SPACE; break;

				case 'e': nBit = WINDRV_OPT_REDUCED_PERIOD; break;
				case 'p': nBit = WINDRV_OPT_REDUCED_PERIODS; break;
				case 'n': nBit = WINDRV_OPT_REDUCED_HYPHEN; break;
				case 'h': nBit = WINDRV_OPT_REDUCED_HYPHENS; break;
				case 'x': nBit = WINDRV_OPT_REDUCED_BADCHAR; break;
				case 's': nBit = WINDRV_OPT_REDUCED_SPACE; break;

				default: break;
			}

			if (nMode)
				nOption |= nBit;
			else
				nOption &= ~nBit;
		}

		pp = p;
	}

	// Set options
	if (nOption != m_nOption) {
		SetOption(nOption);
	}
}

//---------------------------------------------------------------------------
//
/// Get volume label
//
//---------------------------------------------------------------------------
bool CFileSys::FilesVolume(uint32_t nUnit, Human68k::files_t* pFiles) const
{
	assert(pFiles);

	// Get volume label
	TCHAR szVolume[32];
	if (bool bResult = m_cEntry.GetVolumeCache(nUnit, szVolume); !bResult) {
		// Carry out an extra media check here because it may be skipped when doing a manual eject
		if (!m_cEntry.isEnable(nUnit))
			return false;

		// Media check
		if (m_cEntry.isMediaOffline(nUnit))
			return false;

		// Get volume label
		m_cEntry.GetVolume(nUnit, szVolume);
	}
	if (szVolume[0] == '\0')
		return false;

	pFiles->attr = Human68k::AT_VOLUME;
	pFiles->time = 0;
	pFiles->date = 0;
	pFiles->size = 0;

	CHostFilename fname;
	fname.SetHost(szVolume);
	fname.ConvertHuman();
	strcpy((char*)pFiles->full, (const char*)fname.GetHuman());

	return true;
}
