//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
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
#include "../rascsi.h"

//---------------------------------------------------------------------------
//
//  Kanji code conversion
//
//---------------------------------------------------------------------------
#define IC_BUF_SIZE 1024
static char convert_buf[IC_BUF_SIZE];
#ifndef __NetBSD__
// Using POSIX.1 compliant iconv(3)
#define CONVERT(src, dest, inbuf, outbuf, outsize) \
	convert(src, dest, (char *)inbuf, outbuf, outsize)
static void convert(char const *src, char const *dest,
	char *inbuf, char *outbuf, size_t outsize)
#else
// Using NetBSD version of iconv(3): The second argument is 'const char **'
#define CONVERT(src, dest, inbuf, outbuf, outsize) \
	convert(src, dest, inbuf, outbuf, outsize)
static void convert(char const *src, char const *dest,
	const char *inbuf, char *outbuf, size_t outsize)
#endif
{
#ifndef __APPLE__
	*outbuf = '\0';
	size_t in = strlen(inbuf);
	size_t out = outsize - 1;

	iconv_t cd = iconv_open(dest, src);
	if (cd == (iconv_t)-1) {
		return;
	}

	size_t ret = iconv(cd, &inbuf, &in, &outbuf, &out);
	if (ret == (size_t)-1) {
		return;
	}

	iconv_close(cd);
	*outbuf = '\0';
#endif //ifndef __APPLE__
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
	ASSERT(szPath);

	BYTE* p = szPath;
	for (size_t i = 0; i < 65; i++) {
		BYTE c = path[i];
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
	ASSERT(szFilename);

	size_t i;
	BYTE* p = szFilename;

	// Transfer the file name payload
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

CHostDrv::CHostDrv()
{
	m_bWriteProtect = FALSE;
	m_bEnable = FALSE;
	m_capCache.sectors = 0;
	m_bVolumeCache = FALSE;
	m_szVolumeCache[0] = _T('\0');
	m_szBase[0] = _T('\0');
	m_nRing = 0;
}

CHostDrv::~CHostDrv()
{
	CHostPath* p;
	while ((p = (CHostPath*)m_cRing.Next()) != &m_cRing) {
		delete p;
		ASSERT(m_nRing);
		m_nRing--;
	}

	//  Confirm that the entity does not exist (just in case)
	ASSERT(m_cRing.Next() == &m_cRing);
	ASSERT(m_cRing.Prev() == &m_cRing);
	ASSERT(m_nRing == 0);
}

//---------------------------------------------------------------------------
//
// Initialization (device boot and load)
//
//---------------------------------------------------------------------------
void CHostDrv::Init(const TCHAR* szBase, DWORD nFlag)
{
	ASSERT(szBase);
	ASSERT(strlen(szBase) < FILEPATH_MAX);
	ASSERT(m_bWriteProtect == FALSE);
	ASSERT(m_bEnable == FALSE);
	ASSERT(m_capCache.sectors == 0);
	ASSERT(m_bVolumeCache == FALSE);
	ASSERT(m_szVolumeCache[0] == _T('\0'));

	// Confirm that the entity does not exist (just in case)
	ASSERT(m_cRing.Next() == &m_cRing);
	ASSERT(m_cRing.Prev() == &m_cRing);
	ASSERT(m_nRing == 0);

	// Receive parameters
	if (nFlag & FSFLAG_WRITE_PROTECT)
		m_bWriteProtect = TRUE;
	strcpy(m_szBase, szBase);

	// Remove the last path delimiter in the base path
	// @warning needs to be modified when using Unicode
	TCHAR* pClear = NULL;
	TCHAR* p = m_szBase;
	for (;;) {
		TCHAR c = *p;
		if (c == _T('\0'))
			break;
		if (c == _T('/') || c == _T('\\')) {
			pClear = p;
		} else {
			pClear = NULL;
		}
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// To be precise: 0x81~0x9F 0xE0~0xEF
			p++;
			if (*p == _T('\0'))
				break;
		}
		p++;
	}
	if (pClear)
		*pClear = _T('\0');

	// Status update
	m_bEnable = TRUE;
}

//---------------------------------------------------------------------------
//
// Media check
//
//---------------------------------------------------------------------------
BOOL CHostDrv::isMediaOffline()
{
	// Offline status check
	return m_bEnable == FALSE;
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
DWORD CHostDrv::GetStatus() const
{
	return 0x40 | (m_bEnable ? (m_bWriteProtect ? 0x08 : 0) | 0x02 : 0);
}

//---------------------------------------------------------------------------
//
// Media status settings
//
//---------------------------------------------------------------------------
void CHostDrv::SetEnable(BOOL bEnable)
{
	m_bEnable = bEnable;

	if (bEnable == FALSE) {
		// Clear cache
		m_capCache.sectors = 0;
		m_bVolumeCache = FALSE;
		m_szVolumeCache[0] = _T('\0');
	}
}

//---------------------------------------------------------------------------
//
// Media change check
//
//---------------------------------------------------------------------------
BOOL CHostDrv::CheckMedia()
{
	// Status update
	Update();
	if (m_bEnable == FALSE)
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
	BOOL bEnable = TRUE;

	// Media status reflected
	SetEnable(bEnable);
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
	SetEnable(FALSE);

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
	ASSERT(szLabel);
	ASSERT(m_bEnable);

	// Get volume label
	strcpy(m_szVolumeCache, "RASDRV ");
	if (m_szBase[0]) {
		strcat(m_szVolumeCache, m_szBase);
	} else {
		strcat(m_szVolumeCache, "/");
	}

	// Cache update
	m_bVolumeCache = TRUE;

	// Transfer content
	strcpy(szLabel, m_szVolumeCache);
}

//---------------------------------------------------------------------------
//
/// Get volume label from cache
///
/// Transfer the cached volume label information.
/// If the cache contents are valid return TRUE, if invalid return FALSE.
//
//---------------------------------------------------------------------------
BOOL CHostDrv::GetVolumeCache(TCHAR* szLabel) const
{
	ASSERT(szLabel);

	// Transfer contents
	strcpy(szLabel, m_szVolumeCache);

	return m_bVolumeCache;
}

//---------------------------------------------------------------------------
//
/// Get Capacity
//
//---------------------------------------------------------------------------
DWORD CHostDrv::GetCapacity(Human68k::capacity_t* pCapacity)
{
	ASSERT(pCapacity);
	ASSERT(m_bEnable);

	DWORD nFree = 0x7FFF8000;
	DWORD freearea;
	DWORD clusters;
	DWORD sectors;

	freearea = 0xFFFF;
	clusters = 0xFFFF;
	sectors = 64;

	// Estimated parameter range
	ASSERT(freearea <= 0xFFFF);
	ASSERT(clusters <= 0xFFFF);
	ASSERT(sectors <= 64);

	// Update cache
	m_capCache.freearea = (WORD)freearea;
	m_capCache.clusters = (WORD)clusters;
	m_capCache.sectors = (WORD)sectors;
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
/// If the contents of the cache is valid return TRUE, is invalid return FALSE.
//
//---------------------------------------------------------------------------
BOOL CHostDrv::GetCapacityCache(Human68k::capacity_t* pCapacity) const
{
	ASSERT(pCapacity);

	// Transfer contents
	memcpy(pCapacity, &m_capCache, sizeof(m_capCache));

	return m_capCache.sectors != 0;
}

//---------------------------------------------------------------------------
//
/// Update all cache
//
//---------------------------------------------------------------------------
void CHostDrv::CleanCache()
{
	Lock();
	for (CHostPath* p = (CHostPath*)m_cRing.Next(); p != &m_cRing;) {
		p->Release();
		p = (CHostPath*)p->Next();
	}
	Unlock();
}

//---------------------------------------------------------------------------
//
/// Update the cache for the specified path
//
//---------------------------------------------------------------------------
void CHostDrv::CleanCache(const BYTE* szHumanPath)
{
	ASSERT(szHumanPath);

	Lock();
	CHostPath* p = FindCache(szHumanPath);
	if (p) {
		p->Restore();
		p->Release();
	}
	Unlock();
}

//---------------------------------------------------------------------------
//
/// Update the cache below and including the specified path
//
//---------------------------------------------------------------------------
void CHostDrv::CleanCacheChild(const BYTE* szHumanPath)
{
	ASSERT(szHumanPath);

	Lock();
	CHostPath* p = (CHostPath*)m_cRing.Next();
	while (p != &m_cRing) {
		if (p->isSameChild(szHumanPath))
			p->Release();
		p = (CHostPath*)p->Next();
	}
	Unlock();
}

//---------------------------------------------------------------------------
//
/// Delete the cache for the specified path
//
//---------------------------------------------------------------------------
void CHostDrv::DeleteCache(const BYTE* szHumanPath)
{
	ASSERT(szHumanPath);

	Lock();
	CHostPath* p = FindCache(szHumanPath);
	if (p) {
		delete p;
		ASSERT(m_nRing);
		m_nRing--;
	}
	Unlock();
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
	ASSERT(szHuman);

	// Search for something that matches perfectly with either of the stored file names
	for (CHostPath* p = (CHostPath*)m_cRing.Next(); p != &m_cRing;) {
		if (p->isSameHuman(szHuman))
			return p;
		p = (CHostPath*)p->Next();
	}

	return NULL;
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
	ASSERT(pFiles);
	ASSERT(strlen((const char*)pFiles->GetHumanPath()) < HUMAN68K_PATH_MAX);

	// Search in cache
	CHostPath* pPath = FindCache(pFiles->GetHumanPath());
	if (pPath == NULL) {
		return NULL;	// Error: No cache
	}

	// Move to the beginning of the ring
	pPath->Insert(&m_cRing);

	// Cache update check
	if (pPath->isRefresh()) {
		return NULL;	// Error: Cache update is required
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
/// Make it NULL is an error is thrown.
//
//---------------------------------------------------------------------------
CHostPath* CHostDrv::MakeCache(CHostFiles* pFiles)
{
	ASSERT(pFiles);
	ASSERT(strlen((const char*)pFiles->GetHumanPath()) < HUMAN68K_PATH_MAX);

	ASSERT(m_szBase);
	ASSERT(strlen(m_szBase) < FILEPATH_MAX);

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
			return NULL;				// Error: The Human68k path is too long
		szHumanPath[nHumanPath++] = '/';
		szHumanPath[nHumanPath] = '\0';
		if (nHostPath + 1 >= FILEPATH_MAX)
			return NULL;				// Error: The host side path is too long
		szHostPath[nHostPath++] = _T('/');
		szHostPath[nHostPath] = _T('\0');

		// Insert one file
		BYTE szHumanFilename[24];		// File name part
		p = SeparateCopyFilename(p, szHumanFilename);
		if (p == NULL)
			return NULL;				// Error: Failed to read file name
		size_t n = strlen((const char*)szHumanFilename);
		if (nHumanPath + n >= HUMAN68K_PATH_MAX)
			return NULL;				// Error: The Human68k path is too long

		// Is the relevant path cached?
		pPath = FindCache(szHumanPath);
		if (pPath == NULL) {
			// Check for max number of cache
			if (m_nRing >= XM6_HOST_DIRENTRY_CACHE_MAX) {
				// Destroy the oldest cache and reuse it
				pPath = (CHostPath*)m_cRing.Prev();
				pPath->Clean();			// Release all files. Release update check handlers.
			} else {
				// Register new
				pPath = new CHostPath;
				ASSERT(pPath);
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

		// Search the next path
		// Confirm if directory from the middle of the path
		const CHostFilename* pFilename;
		if (*p != '\0')
			pFilename = pPath->FindFilename(szHumanFilename, Human68k::AT_DIRECTORY);
		else
			pFilename = pPath->FindFilename(szHumanFilename);
		if (pFilename == NULL)
			return NULL;				// Error: Could not find path or file names in the middle

		// Link path name
		strcpy((char*)szHumanPath + nHumanPath, (const char*)szHumanFilename);
		nHumanPath += n;

		n = strlen(pFilename->GetHost());
		if (nHostPath + n >= FILEPATH_MAX)
			return NULL;				// Error: Host side path is too long
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
/// Search for host side name (path name + file name (can be abbeviated) + attribute)
///
/// Set all Human68k parameters once more.
//
//---------------------------------------------------------------------------
BOOL CHostDrv::Find(CHostFiles* pFiles)
{
	ASSERT(pFiles);

	Lock();

	// Get path name and build cache
	CHostPath* pPath = CopyCache(pFiles);
	if (pPath == NULL) {
		pPath = MakeCache(pFiles);
		if (pPath == NULL) {
			Unlock();
			CleanCache();
			return FALSE;	// Error: Failed to build cache
		}
	}

	// Store host side path
	pFiles->SetResult(pPath->GetHost());

	// Exit if only path name
	if (pFiles->isPathOnly()) {
		Unlock();
		return TRUE;		// Normal exit: only path name
	}

	// Search file name
	const CHostFilename* pFilename = pFiles->Find(pPath);
	if (pFilename == NULL) {
		Unlock();
		return FALSE;		// Error: Could not get file name
	}

	// Store the Human68k side search results
	pFiles->SetEntry(pFilename);

	// Store the host side full path name
	pFiles->AddResult(pFilename->GetHost());

	Unlock();

	return TRUE;
}

//===========================================================================
//
//	Directory entry: File name
//
//===========================================================================

CHostFilename::CHostFilename()
{
	m_bCorrect = FALSE;
	m_pszHumanExt = FALSE;
	m_pszHumanLast = FALSE;
}

//---------------------------------------------------------------------------
//
/// Set host side name
//
//---------------------------------------------------------------------------
void CHostFilename::SetHost(const TCHAR* szHost)
{
	ASSERT(szHost);
	ASSERT(strlen(szHost) < FILEPATH_MAX);

	strcpy(m_szHost, szHost);
}

//---------------------------------------------------------------------------
//
/// Copy the Human68k file name elements
//
//---------------------------------------------------------------------------
BYTE* CHostFilename::CopyName(BYTE* pWrite, const BYTE* pFirst, const BYTE* pLast)	// static
{
	ASSERT(pWrite);
	ASSERT(pFirst);
	ASSERT(pLast);

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
	char szHost[FILEPATH_MAX];


	// Don't do conversion for special directory names
	if (m_szHost[0] == _T('.') &&
		(m_szHost[1] == _T('\0') || (m_szHost[1] == _T('.') && m_szHost[2] == _T('\0')))) {
		strcpy((char*)m_szHuman, m_szHost);

		m_bCorrect = TRUE;
		m_pszHumanLast = m_szHuman + strlen((const char*)m_szHuman);
		m_pszHumanExt = m_pszHumanLast;
		return;
	}

	size_t nMax = 18;	// Number of bytes for the base segment (base name and extension)
	DWORD nOption = CFileSys::GetFileOption();
	if (nOption & WINDRV_OPT_CONVERT_LENGTH)
		nMax = 8;

	// Preparations to adjust the base name segment
	BYTE szNumber[8];
	BYTE* pNumber = NULL;
	if (nCount >= 0) {
		pNumber = &szNumber[8];
		for (DWORD i = 0; i < 5; i++) {	// Max 5+1 digits (always leave the first 2 bytes of the base name)
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
		BYTE c = (BYTE)((nOption >> 24) & 0x7F);
		if (c == 0)
			c = XM6_HOST_FILENAME_MARK;
		*pNumber = c;
	}

	// Char conversion
	BYTE szHuman[FILEPATH_MAX];
	const BYTE* pFirst = szHuman;
	const BYTE* pLast;
	const BYTE* pExt = NULL;

	{
		strcpy(szHost, m_szHost);
		const BYTE* pRead = (const BYTE*)szHost;
		BYTE* pWrite = szHuman;
		const BYTE* pPeriod = SeparateExt(pRead);

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
			BYTE* p = (BYTE*)pLast;
			*p = '\0';
		}

		// Delete if the file name disappeared after conversion
		if (pExt + 1 >= pLast) {
			pLast = pExt;
			BYTE* p = (BYTE*)pLast;
			*p = '\0';		// Just in case
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
	pCut = (BYTE*)strchr((const char*)pCut, '.');	// The 2nd byte of Shift-JIS is always 0x40 or higher, so this is ok
	if (pCut == NULL)
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
	size_t nExt = pExt - pSecond;	// Length of extension segment
	if ((size_t)(pCut - pFirst) + nExt > nMax)
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
	m_bCorrect = TRUE;

	// Fail if the base file name does not exist
	if (m_pszHumanExt <= m_szHuman)
		m_bCorrect = FALSE;

	// Fail if the base file name is more than 1 char and ends with a space
	// While it is theoretically valid to have a base file name exceed 8 chars,
	// Human68k is unable to handle it, so failing this case too.
	else if (m_pszHumanExt[-1] == ' ')
		m_bCorrect = FALSE;

	// Fail if the conversion result is the same as a special directory name
	if (m_szHuman[0] == '.' &&
		(m_szHuman[1] == '\0' || (m_szHuman[1] == '.' && m_szHuman[2] == '\0')))
		m_bCorrect = FALSE;
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
	ASSERT(szHuman);
	ASSERT(strlen((const char*)szHuman) < 23);

	strcpy((char*)m_szHuman, (const char*)szHuman);
	m_bCorrect = TRUE;
	m_pszHumanLast = m_szHuman + strlen((const char*)m_szHuman);
	m_pszHumanExt = (BYTE*)SeparateExt(m_szHuman);
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
	BYTE* p = m_szHuman;
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
BOOL CHostFilename::isReduce() const
{

	return strcmp((char *)m_szHost, (const char*)m_szHuman) != 0;
}

//---------------------------------------------------------------------------
//
/// Evaluate Human68k directory entry attribute
//
//---------------------------------------------------------------------------
BOOL CHostFilename::CheckAttribute(DWORD nHumanAttribute) const
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
	const BYTE* pExt = (BYTE*)strrchr((const char*)pFirst, '.');	// The 2nd byte of Shift-JIS is always 0x40 or higher, so this is ok
	if (pExt == NULL)
		pExt = pLast;
	// Special handling of the pattern where the file name is 20~22 chars, and the 19th char is '.' or ends with '.'
	if (20 <= nLength && nLength <= 22 && pFirst[18] == '.' && pFirst[nLength - 1] == '.')
		pExt = pFirst + 18;
	// Calculate the number of chars in the extension (-1:None 0:Only period 1~3:Human68k extension 4 or above:extension name)
	size_t nExt = pLast - pExt - 1;
	// Consider it an extension only when '.' is anywhere except the beginning of the string, and between 1~3 chars long
	if (pExt == pFirst || nExt < 1 || nExt > 3)
		pExt = pLast;

	return pExt;
}

//===========================================================================
//
//	Directory entry: path name
//
//===========================================================================

DWORD CHostPath::g_nId;				///< Identifier creation counter

CHostPath::CHostPath()
{
	m_bRefresh = TRUE;
	m_nId = 0;
	m_tBackup = FALSE;

#ifdef _DEBUG
	// Initialization is not required because this value always gets updated (used for debugging or initialization operation)
	m_nId = 0;
#endif	// _DEBUG
}

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
	ASSERT(nLength < FILEPATH_MAX);

	size_t n = offsetof(ring_t, f) + CHostFilename::Offset() + (nLength + 1) * sizeof(TCHAR);
	ring_t* p = (ring_t*)malloc(n);
	ASSERT(p);

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
	ASSERT(pRing);

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
	ASSERT(szHuman);
	ASSERT(strlen((const char*)szHuman) < HUMAN68K_PATH_MAX);

	strcpy((char*)m_szHuman, (const char*)szHuman);
}

//---------------------------------------------------------------------------
//
/// Specify host side names directly
//
//---------------------------------------------------------------------------
void CHostPath::SetHost(const TCHAR* szHost)
{
	ASSERT(szHost);
	ASSERT(strlen(szHost) < FILEPATH_MAX);

	strcpy(m_szHost, szHost);
}

//---------------------------------------------------------------------------
//
/// Compare arrays (supports wild cards)
//
//---------------------------------------------------------------------------
int CHostPath::Compare(const BYTE* pFirst, const BYTE* pLast, const BYTE* pBufFirst, const BYTE* pBufLast)
{
	ASSERT(pFirst);
	ASSERT(pLast);
	ASSERT(pBufFirst);
	ASSERT(pBufLast);

	// Compare chars
	BOOL bSkip0 = FALSE;
	BOOL bSkip1 = FALSE;
	for (const BYTE* p = pFirst; p < pLast; p++) {
		// Read 1 char
		BYTE c = *p;
		BYTE d = '\0';
		if (pBufFirst < pBufLast)
			d = *pBufFirst++;

		// Ajust char for comparison
		if (bSkip0 == FALSE) {
			if (bSkip1 == FALSE) {	// First byte for both c and d
				if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// Specifically 0x81~0x9F 0xE0~0xEF
					bSkip0 = TRUE;
				}
				if ((0x80 <= d && d <= 0x9F) || 0xE0 <= d) {	// Specifically 0x81~0x9F 0xE0~0xEF
					bSkip1 = TRUE;
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
					bSkip0 = TRUE;
				}
				bSkip1 = FALSE;
			}
		} else {
			if (bSkip1 == FALSE) {	// Only d is first byte
				bSkip0 = FALSE;
				if ((0x80 <= d && d <= 0x9F) || 0xE0 <= d) {	// Specifically 0x81~0x9F 0xE0~0xEF
					bSkip1 = TRUE;
				}
			} else {		// Second byte for both c and d
				bSkip0 = FALSE;
				bSkip1 = FALSE;
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
/// Human68k側の名称を比較する
//
//---------------------------------------------------------------------------
BOOL CHostPath::isSameHuman(const BYTE* szHuman) const
{
	ASSERT(szHuman);

	// 文字数計算
	size_t nLength = strlen((const char*)m_szHuman);
	size_t n = strlen((const char*)szHuman);

	// 文字数チェック
	if (nLength != n)
		return FALSE;

	// Human68kパス名の比較
	return Compare(m_szHuman, m_szHuman + nLength, szHuman, szHuman + n) == 0;
}

//---------------------------------------------------------------------------
//
/// Human68k側の名称を比較する
//
//---------------------------------------------------------------------------
BOOL CHostPath::isSameChild(const BYTE* szHuman) const
{
	ASSERT(szHuman);

	// 文字数計算
	size_t nLength = strlen((const char*)m_szHuman);
	size_t n = strlen((const char*)szHuman);

	// 文字数チェック
	if (nLength < n)
		return FALSE;

	// Human68kパス名の比較
	return Compare(m_szHuman, m_szHuman + n, szHuman, szHuman + n) == 0;
}

//---------------------------------------------------------------------------
//
/// ファイル名を検索
///
/// 所有するキャシュバッファの中から検索し、見つかればその名称を返す。
/// パス名を除外しておくこと。
/// 必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
const CHostFilename* CHostPath::FindFilename(const BYTE* szHuman, DWORD nHumanAttribute) const
{
	ASSERT(szHuman);

	// 文字数計算
	const BYTE* pFirst = szHuman;
	size_t nLength = strlen((const char*)pFirst);
	const BYTE* pLast = pFirst + nLength;

	// 所持している全てのファイル名の中から完全一致するものを検索
	const ring_t* p = (ring_t*)m_cRing.Next();
	for (; p != (ring_t*)&m_cRing; p = (ring_t*)p->r.Next()) {
		// 属性チェック
		if (p->f.CheckAttribute(nHumanAttribute) == 0)
			continue;
		// 文字数計算
		const BYTE* pBufFirst = p->f.GetHuman();
		const BYTE* pBufLast = p->f.GetHumanLast();
		size_t nBufLength = pBufLast - pBufFirst;
		// 文字数チェック
		if (nLength != nBufLength)
			continue;
		// ファイル名チェック
		if (Compare(pFirst, pLast, pBufFirst, pBufLast) == 0)
			return &p->f;
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
/// ファイル名を検索 (ワイルドカード対応)
///
/// 所有するバッファの中から検索し、見つかればその名称を返す。
/// パス名を除外しておくこと。
/// 必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
const CHostFilename* CHostPath::FindFilenameWildcard(const BYTE* szHuman, DWORD nHumanAttribute, find_t* pFind) const
{
	ASSERT(szHuman);
	ASSERT(pFind);

	// 検索ファイル名を本体とHuman68k拡張子に分ける
	const BYTE* pFirst = szHuman;
	const BYTE* pLast = pFirst + strlen((const char*)pFirst);
	const BYTE* pExt = CHostFilename::SeparateExt(pFirst);

	// 開始地点へ移動
	const ring_t* p = (ring_t*)m_cRing.Next();
	if (pFind->count > 0) {
		if (pFind->id == m_nId) {
			// ディレクトリエントリが同一なら、前回の位置から即継続
			p = pFind->pos;
		} else {
			// 開始地点をディレクトリエントリ内容から検索する
			DWORD n = 0;
			for (;; p = (ring_t*)p->r.Next()) {
				if (p == (ring_t*)&m_cRing) {
					// 同一エントリが見つからなかった場合、回数から推定 (念のため)
					p = (ring_t*)m_cRing.Next();
					n = 0;
					for (; p != (ring_t*)&m_cRing; p = (ring_t*)p->r.Next()) {
						if (n >= pFind->count)
							break;
						n++;
					}
					break;
				}
				if (p->f.isSameEntry(&pFind->entry)) {
					// 同一エントリを発見
					pFind->count = n;
					break;
				}
				n++;
			}
		}
	}

	// ファイル検索
	for (; p != (ring_t*)&m_cRing; p = (ring_t*)p->r.Next()) {
		pFind->count++;

		// 属性チェック
		if (p->f.CheckAttribute(nHumanAttribute) == 0)
			continue;

		// ファイル名を本体とHuman68k拡張子に分ける
		const BYTE* pBufFirst = p->f.GetHuman();
		const BYTE* pBufLast = p->f.GetHumanLast();
		const BYTE* pBufExt = p->f.GetHumanExt();

		// 本体比較
		if (Compare(pFirst, pExt, pBufFirst, pBufExt))
			continue;

		// Human68k拡張子比較
		// 拡張子.???の場合は、Human68k拡張子のピリオドなしにもマッチさせる
		if (strcmp((const char*)pExt, ".???") == 0 ||
			Compare(pExt, pLast, pBufExt, pBufLast) == 0) {
			// 次の候補のディレクトリエントリ内容を記録
			const ring_t* pNext = (ring_t*)p->r.Next();
			pFind->id = m_nId;
			pFind->pos = pNext;
			if (pNext != (ring_t*)&m_cRing)
				memcpy(&pFind->entry, pNext->f.GetEntry(), sizeof(pFind->entry));
			else
				memset(&pFind->entry, 0, sizeof(pFind->entry));
			return &p->f;
		}
	}

	pFind->id = m_nId;
	pFind->pos = p;
	memset(&pFind->entry, 0, sizeof(pFind->entry));
	return NULL;
}

//---------------------------------------------------------------------------
//
/// ファイル変更が行なわれたか確認
//
//---------------------------------------------------------------------------
BOOL CHostPath::isRefresh()
{

	return m_bRefresh;
}

//---------------------------------------------------------------------------
//
/// ASCIIソート関数
//
//---------------------------------------------------------------------------
int AsciiSort(const dirent **a, const dirent **b)
{
	return strcmp((*a)->d_name, (*b)->d_name);
}

//---------------------------------------------------------------------------
//
/// ファイル再構成
///
/// ここで初めて、ホスト側のファイルシステムの観測が行なわれる。
/// 必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
void CHostPath::Refresh()
{
	ASSERT(strlen(m_szHost) + 22 < FILEPATH_MAX);

	// タイムスタンプ保存
	Backup();

	TCHAR szPath[FILEPATH_MAX];
	strcpy(szPath, m_szHost);

	// 更新フラグ変更
	m_bRefresh = FALSE;

	// 以前のキャッシュ内容を保存
	CRing cRingBackup;
	m_cRing.InsertRing(&cRingBackup);

	// ファイル名登録
	/// @todo ファイル重複処理をホスト側APIを経由せずに全て自前で処理する。
	BOOL bUpdate = FALSE;
	struct dirent **pd = NULL;
	int nument = 0;
	int maxent = XM6_HOST_DIRENTRY_FILE_MAX;
	for (int i = 0; i < maxent; i++) {
		TCHAR szFilename[FILEPATH_MAX];
		if (pd == NULL) {
			nument = scandir(S2U(szPath), &pd, NULL, AsciiSort);
			if (nument == -1) {
				pd = NULL;
				break;
			}
			maxent = nument;
		}

		// 最上位ディレクトリならカレントとパレントを対象外とする
		struct dirent* pe = pd[i];
		if (m_szHuman[0] == '/' && m_szHuman[1] == 0) {
			if (strcmp(pe->d_name, ".") == 0 || strcmp(pe->d_name, "..") == 0) {
				continue;
			}
		}

		// ファイル名を獲得
		strcpy(szFilename, U2S(pe->d_name));

		// ファイル名領域確保
		ring_t* pRing = Alloc(strlen(szFilename));
		CHostFilename* pFilename = &pRing->f;
		pFilename->SetHost(szFilename);

		// 以前のキャッシュ内容に該当するファイル名があればそのHuman68k名称を優先する
		ring_t* pCache = (ring_t*)cRingBackup.Next();
		for (;;) {
			if (pCache == (ring_t*)&cRingBackup) {
				pCache = NULL;			// 該当するエントリなし
				bUpdate = TRUE;			// 新規エントリと確定
				pFilename->ConvertHuman();
				break;
			}
			if (strcmp(pFilename->GetHost(), pCache->f.GetHost()) == 0) {
				pFilename->CopyHuman(pCache->f.GetHuman());	// Human68k名称のコピー
				break;
			}
			pCache = (ring_t*)pCache->r.Next();
		}

		// 新規エントリの場合はファイル名重複をチェックする
		// ホスト側のファイル名から変更があったか、Human68kで表現できないファイル名の場合は
		// 以下のチェックを全てパスするファイル名を新たに生成する
		// ・正しいファイル名であること
		// ・過去のエントリに同名のものが存在しないこと
		// ・同名の実ファイル名が存在しないこと
		if (pFilename->isReduce() || !pFilename->isCorrect()) {	// ファイル名変更が必要か確認
			for (DWORD n = 0; n < XM6_HOST_FILENAME_PATTERN_MAX; n++) {
				// 正しいファイル名かどうか確認
				if (pFilename->isCorrect()) {
					// 過去のエントリと一致するか確認
					const CHostFilename* pCheck = FindFilename(pFilename->GetHuman());
					if (pCheck == NULL) {
						// 一致するものがなければ、実ファイルが存在するか確認
						strcpy(szPath, m_szHost);
						strcat(szPath, (const char*)pFilename->GetHuman());
						struct stat sb;
						if (stat(S2U(szPath), &sb))
							break;	// 利用可能パターンを発見
					}
				}
				// 新しい名前を生成
				pFilename->ConvertHuman(n);
			}
		}

		// ディレクトリエントリ名称
		pFilename->SetEntryName();

		// 情報取得
		strcpy(szPath, m_szHost);
		strcat(szPath, U2S(pe->d_name));

		struct stat sb;
		if (stat(S2U(szPath), &sb))
			continue;

		// 属性
		BYTE nHumanAttribute = Human68k::AT_ARCHIVE;
		if (S_ISDIR(sb.st_mode))
			nHumanAttribute = Human68k::AT_DIRECTORY;
		if ((sb.st_mode & 0200) == 0)
			nHumanAttribute |= Human68k::AT_READONLY;
		pFilename->SetEntryAttribute(nHumanAttribute);

		// サイズ
		DWORD nHumanSize = (DWORD)sb.st_size;
		pFilename->SetEntrySize(nHumanSize);

		// 日付時刻
		WORD nHumanDate = 0;
		WORD nHumanTime = 0;
		struct tm* pt = localtime(&sb.st_mtime);
		if (pt) {
			nHumanDate = (WORD)(((pt->tm_year - 80) << 9) | ((pt->tm_mon + 1) << 5) | pt->tm_mday);
			nHumanTime = (WORD)((pt->tm_hour << 11) | (pt->tm_min << 5) | (pt->tm_sec >> 1));
		}
		pFilename->SetEntryDate(nHumanDate);
		pFilename->SetEntryTime(nHumanTime);

		// クラスタ番号設定
		pFilename->SetEntryCluster(0);

		// 以前のキャッシュ内容と比較
		if (pCache) {
			if (pCache->f.isSameEntry(pFilename->GetEntry())) {
				Free(pRing);			// 今回作成したエントリは破棄し
				pRing = pCache;			// 以前のキャッシュ内容を使う
			} else {
				Free(pCache);			// 次回の検索対象から除外
				bUpdate = TRUE;			// 一致しなければ更新あり
			}
		}

		// リング末尾へ追加
		pRing->r.InsertTail(&m_cRing);
	}

	// ディレクトリエントリを解放
	if (pd) {
		for (int i = 0; i < nument; i++) {
			free(pd[i]);
		}
		free(pd);
	}

	// 残存するキャッシュ内容を削除
	ring_t* p;
	while ((p = (ring_t*)cRingBackup.Next()) != (ring_t*)&cRingBackup) {
		bUpdate = TRUE;					// 削除によってエントリ数の減少が判明
		Free(p);
	}

	// 更新が行なわれたら識別IDを変更
	if (bUpdate)
		m_nId = ++g_nId;
	//	ASSERT(m_nId);
}

//---------------------------------------------------------------------------
//
/// ホスト側のタイムスタンプを保存
//
//---------------------------------------------------------------------------
void CHostPath::Backup()
{
	ASSERT(m_szHost);
	ASSERT(strlen(m_szHost) < FILEPATH_MAX);

	TCHAR szPath[FILEPATH_MAX];
	strcpy(szPath, m_szHost);
	size_t len = strlen(szPath);

	m_tBackup = 0;
	if (len > 1) {	// ルートディレクトリの場合は何もしない
		len--;
		ASSERT(szPath[len] == _T('/'));
		szPath[len] = _T('\0');
		struct stat sb;
		if (stat(S2U(szPath), &sb) == 0)
			m_tBackup = sb.st_mtime;
	}
}

//---------------------------------------------------------------------------
//
/// ホスト側のタイムスタンプを復元
//
//---------------------------------------------------------------------------
void CHostPath::Restore() const
{
	ASSERT(m_szHost);
	ASSERT(strlen(m_szHost) < FILEPATH_MAX);

	TCHAR szPath[FILEPATH_MAX];
	strcpy(szPath, m_szHost);
	size_t len = strlen(szPath);

	if (m_tBackup) {
		ASSERT(len);
		len--;
		ASSERT(szPath[len] == _T('/'));
		szPath[len] = _T('\0');

		struct utimbuf ut;
		ut.actime = m_tBackup;
		ut.modtime = m_tBackup;
		utime(szPath, &ut);
	}
}

//---------------------------------------------------------------------------
//
/// 更新
//
//---------------------------------------------------------------------------
void CHostPath::Release()
{

	m_bRefresh = TRUE;
}

//===========================================================================
//
//	ディレクトリエントリ管理
//
//===========================================================================

//---------------------------------------------------------------------------
//
/// デフォルトコンストラクタ
//
//---------------------------------------------------------------------------
CHostEntry::CHostEntry()
{
	for (size_t n = 0; n < DriveMax; n++) {
		m_pDrv[n] = NULL;
	}

	m_nTimeout = 0;
}

//---------------------------------------------------------------------------
//
/// デストラクタ final
//
//---------------------------------------------------------------------------
CHostEntry::~CHostEntry()
{
	Clean();

#ifdef _DEBUG
	// オブジェクト確認
	for (size_t n = 0; n < DriveMax; n++) {
		ASSERT(m_pDrv[n] == NULL);
	}
#endif	// _DEBUG
}

//---------------------------------------------------------------------------
//
/// 初期化 (ドライバ組込み時)
//
//---------------------------------------------------------------------------
void CHostEntry::Init()
{

#ifdef _DEBUG
	// オブジェクト確認
	for (size_t n = 0; n < DriveMax; n++) {
		ASSERT(m_pDrv[n] == NULL);
	}
#endif	// _DEBUG
}

//---------------------------------------------------------------------------
//
/// 解放 (起動・リセット時)
//
//---------------------------------------------------------------------------
void CHostEntry::Clean()
{

	// オブジェクト削除
	for (size_t n = 0; n < DriveMax; n++) {
		delete m_pDrv[n];
		m_pDrv[n] = NULL;
	}
}

//---------------------------------------------------------------------------
//
/// 全てのキャッシュを更新する
//
//---------------------------------------------------------------------------
void CHostEntry::CleanCache()
{

	for (size_t i = 0; i < DriveMax; i++) {
		if (m_pDrv[i])
			m_pDrv[i]->CleanCache();
	}

	CHostPath::InitId();
}

//---------------------------------------------------------------------------
//
/// 指定されたユニットのキャッシュを更新する
//
//---------------------------------------------------------------------------
void CHostEntry::CleanCache(DWORD nUnit)
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	m_pDrv[nUnit]->CleanCache();
}

//---------------------------------------------------------------------------
//
/// 指定されたパスのキャッシュを更新する
//
//---------------------------------------------------------------------------
void CHostEntry::CleanCache(DWORD nUnit, const BYTE* szHumanPath)
{
	ASSERT(szHumanPath);
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	m_pDrv[nUnit]->CleanCache(szHumanPath);
}

//---------------------------------------------------------------------------
//
/// 指定されたパス以下のキャッシュを全て更新する
//
//---------------------------------------------------------------------------
void CHostEntry::CleanCacheChild(DWORD nUnit, const BYTE* szHumanPath)
{
	ASSERT(szHumanPath);
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	m_pDrv[nUnit]->CleanCacheChild(szHumanPath);
}

//---------------------------------------------------------------------------
//
/// 指定されたパスのキャッシュを削除する
//
//---------------------------------------------------------------------------
void CHostEntry::DeleteCache(DWORD nUnit, const BYTE* szHumanPath)
{
	ASSERT(szHumanPath);
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	m_pDrv[nUnit]->DeleteCache(szHumanPath);
}

//---------------------------------------------------------------------------
//
/// ホスト側の名称を検索 (パス名+ファイル名(省略可)+属性)
//
//---------------------------------------------------------------------------
BOOL CHostEntry::Find(DWORD nUnit, CHostFiles* pFiles)
{
	ASSERT(pFiles);
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->Find(pFiles);
}

void CHostEntry::ShellNotify(DWORD, const TCHAR*) {}

//---------------------------------------------------------------------------
//
/// ドライブ設定
//
//---------------------------------------------------------------------------
void CHostEntry::SetDrv(DWORD nUnit, CHostDrv* pDrv)
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit] == NULL);

	m_pDrv[nUnit] = pDrv;
}

//---------------------------------------------------------------------------
//
/// 書き込み禁止か？
//
//---------------------------------------------------------------------------
BOOL CHostEntry::isWriteProtect(DWORD nUnit) const
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->isWriteProtect();
}

//---------------------------------------------------------------------------
//
/// アクセス可能か？
//
//---------------------------------------------------------------------------
BOOL CHostEntry::isEnable(DWORD nUnit) const
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->isEnable();
}

//---------------------------------------------------------------------------
//
/// メディアチェック
//
//---------------------------------------------------------------------------
BOOL CHostEntry::isMediaOffline(DWORD nUnit)
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->isMediaOffline();
}

//---------------------------------------------------------------------------
//
/// メディアバイトの取得
//
//---------------------------------------------------------------------------
BYTE CHostEntry::GetMediaByte(DWORD nUnit) const
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetMediaByte();
}

//---------------------------------------------------------------------------
//
/// ドライブ状態の取得
//
//---------------------------------------------------------------------------
DWORD CHostEntry::GetStatus(DWORD nUnit) const
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetStatus();
}

//---------------------------------------------------------------------------
//
/// メディア交換チェック
//
//---------------------------------------------------------------------------
BOOL CHostEntry::CheckMedia(DWORD nUnit)
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->CheckMedia();
}

//---------------------------------------------------------------------------
//
/// イジェクト
//
//---------------------------------------------------------------------------
void CHostEntry::Eject(DWORD nUnit)
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	m_pDrv[nUnit]->Eject();
}

//---------------------------------------------------------------------------
//
/// ボリュームラベルの取得
//
//---------------------------------------------------------------------------
void CHostEntry::GetVolume(DWORD nUnit, TCHAR* szLabel)
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	m_pDrv[nUnit]->GetVolume(szLabel);
}

//---------------------------------------------------------------------------
//
/// キャッシュからボリュームラベルを取得
//
//---------------------------------------------------------------------------
BOOL CHostEntry::GetVolumeCache(DWORD nUnit, TCHAR* szLabel) const
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetVolumeCache(szLabel);
}

//---------------------------------------------------------------------------
//
/// 容量の取得
//
//---------------------------------------------------------------------------
DWORD CHostEntry::GetCapacity(DWORD nUnit, Human68k::capacity_t* pCapacity)
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetCapacity(pCapacity);
}

//---------------------------------------------------------------------------
//
/// キャッシュからクラスタサイズを取得
//
//---------------------------------------------------------------------------
BOOL CHostEntry::GetCapacityCache(DWORD nUnit, Human68k::capacity_t* pCapacity) const
{
	ASSERT(nUnit < DriveMax);
	ASSERT(m_pDrv[nUnit]);

	return m_pDrv[nUnit]->GetCapacityCache(pCapacity);
}

//---------------------------------------------------------------------------
//
/// Human68kフルパス名から先頭の要素を分離・コピー
///
/// Human68kフルパス名の先頭の要素をパス区切り文字を除外して取得する。
/// 書き込み先バッファは23バイト必要。
/// Human68kパスは必ず/で開始すること。
/// 途中/が2つ以上連続して出現した場合はエラーとする。
/// 文字列終端が/だけの場合は空の文字列として処理し、エラーにはしない。
//
//---------------------------------------------------------------------------
const BYTE* CHostDrv::SeparateCopyFilename(const BYTE* szHuman, BYTE* szBuffer)		// static
{
	ASSERT(szHuman);
	ASSERT(szBuffer);

	const size_t nMax = 22;
	const BYTE* p = szHuman;

	BYTE c = *p++;				// 読み込み
	if (c != '/' && c != '\\')
		return NULL;			// エラー: 不正なパス名

	// ファイルいっこいれる
	size_t i = 0;
	for (;;) {
		c = *p;					// 読み込み
		if (c == '\0')
			break;				// 文字列終端なら終了 (終端位置を返す)
		if (c == '/' || c == '\\') {
			if (i == 0)
				return NULL;	// エラー: パス区切り文字が連続している
			break;				// パスの区切りを読んだら終了 (文字の位置を返す)
		}
		p++;

		if (i >= nMax)
			return NULL;		// エラー: 1バイト目がバッファ終端にかかる
		szBuffer[i++] = c;		// 書き込み

		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// 厳密には0x81～0x9Fと0xE0～0xEF
			c = *p++;			// 読み込み
			if (c < 0x40)
				return NULL;	// エラー: 不正なSJIS2バイト目

			if (i >= nMax)
				return NULL;	// エラー: 2バイト目がバッファ終端にかかる
			szBuffer[i++] = c;	// 書き込み
		}
	}
	szBuffer[i] = '\0';			// 書き込み

	return p;
}

//===========================================================================
//
//	ファイル検索処理
//
//===========================================================================

//---------------------------------------------------------------------------
//
/// 初期化
//
//---------------------------------------------------------------------------
void CHostFiles::Init()
{
}

//---------------------------------------------------------------------------
//
/// パス名・ファイル名を内部で生成
//
//---------------------------------------------------------------------------
void CHostFiles::SetPath(const Human68k::namests_t* pNamests)
{
	ASSERT(pNamests);

	pNamests->GetCopyPath(m_szHumanPath);
	pNamests->GetCopyFilename(m_szHumanFilename);
	m_nHumanWildcard = 0;
	m_nHumanAttribute = Human68k::AT_ARCHIVE;
	m_findNext.Clear();
}

//---------------------------------------------------------------------------
//
/// Human68k側でファイルを検索しホスト側の情報を生成
//
//---------------------------------------------------------------------------
BOOL CHostFiles::Find(DWORD nUnit, CHostEntry* pEntry)
{
	ASSERT(pEntry);

	return pEntry->Find(nUnit, this);
}

//---------------------------------------------------------------------------
//
/// ファイル名検索
//
//---------------------------------------------------------------------------
const CHostFilename* CHostFiles::Find(CHostPath* pPath)
{
	ASSERT(pPath);

	if (m_nHumanWildcard)
		return pPath->FindFilenameWildcard(m_szHumanFilename, m_nHumanAttribute, &m_findNext);

	return pPath->FindFilename(m_szHumanFilename, m_nHumanAttribute);
}

//---------------------------------------------------------------------------
//
/// Human68k側の検索結果保存
//
//---------------------------------------------------------------------------
void CHostFiles::SetEntry(const CHostFilename* pFilename)
{
	ASSERT(pFilename);

	// Human68kディレクトリエントリ保存
	memcpy(&m_dirHuman, pFilename->GetEntry(), sizeof(m_dirHuman));

	// Human68kファイル名保存
	strcpy((char*)m_szHumanResult, (const char*)pFilename->GetHuman());
}

//---------------------------------------------------------------------------
//
/// ホスト側の名称を設定
//
//---------------------------------------------------------------------------
void CHostFiles::SetResult(const TCHAR* szPath)
{
	ASSERT(szPath);
	ASSERT(strlen(szPath) < FILEPATH_MAX);

	strcpy(m_szHostResult, szPath);
}

//---------------------------------------------------------------------------
//
/// ホスト側の名称にファイル名を追加
//
//---------------------------------------------------------------------------
void CHostFiles::AddResult(const TCHAR* szPath)
{
	ASSERT(szPath);
	ASSERT(strlen(m_szHostResult) + strlen(szPath) < FILEPATH_MAX);

	strcat(m_szHostResult, szPath);
}

//---------------------------------------------------------------------------
//
/// ホスト側の名称にHuman68kの新規ファイル名を追加
//
//---------------------------------------------------------------------------
void CHostFiles::AddFilename()
{
	ASSERT(strlen(m_szHostResult) + strlen((const char*)m_szHumanFilename) < FILEPATH_MAX);
	/// @warning Unicode未対応。いずれUnicodeの世界に飮まれた時はここで変換を行なう → 済
	strncat(m_szHostResult, (const char*)m_szHumanFilename, ARRAY_SIZE(m_szHumanFilename));
}

//===========================================================================
//
//	ファイル検索領域 マネージャ
//
//===========================================================================

#ifdef _DEBUG
//---------------------------------------------------------------------------
//
/// デストラクタ final
//
//---------------------------------------------------------------------------
CHostFilesManager::~CHostFilesManager()
{
	// 実体が存在しないことを確認 (念のため)
	ASSERT(m_cRing.Next() == &m_cRing);
	ASSERT(m_cRing.Prev() == &m_cRing);
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
/// 初期化 (ドライバ組込み時)
//
//---------------------------------------------------------------------------
void CHostFilesManager::Init()
{

	// 実体が存在しないことを確認 (念のため)
	ASSERT(m_cRing.Next() == &m_cRing);
	ASSERT(m_cRing.Prev() == &m_cRing);

	// メモリ確保
	for (DWORD i = 0; i < XM6_HOST_FILES_MAX; i++) {
		ring_t* p = new ring_t;
		ASSERT(p);
		p->r.Insert(&m_cRing);
	}
}

//---------------------------------------------------------------------------
//
/// 解放 (起動・リセット時)
//
//---------------------------------------------------------------------------
void CHostFilesManager::Clean()
{

	// メモリ解放
	CRing* p;
	while ((p = m_cRing.Next()) != &m_cRing) {
		delete (ring_t*)p;
	}
}

//---------------------------------------------------------------------------
//
/// 確保
//
//---------------------------------------------------------------------------
CHostFiles* CHostFilesManager::Alloc(DWORD nKey)
{
	ASSERT(nKey);

	// 末尾から選択
	ring_t* p = (ring_t*)m_cRing.Prev();

	// リング先頭へ移動
	p->r.Insert(&m_cRing);

	// キーを設定
	p->f.SetKey(nKey);

	return &p->f;
}

//---------------------------------------------------------------------------
//
/// 検索
//
//---------------------------------------------------------------------------
CHostFiles* CHostFilesManager::Search(DWORD nKey)
{
	// ASSERT(nKey);	// DPB破損により検索キーが0になることもある

	// 該当するオブジェクトを検索
	ring_t* p = (ring_t*)m_cRing.Next();
	for (; p != (ring_t*)&m_cRing; p = (ring_t*)p->r.Next()) {
		if (p->f.isSameKey(nKey)) {
			// リング先頭へ移動
			p->r.Insert(&m_cRing);
			return &p->f;
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
/// 解放
//
//---------------------------------------------------------------------------
void CHostFilesManager::Free(CHostFiles* pFiles)
{
	ASSERT(pFiles);

	// 解放
	pFiles->SetKey(0);
	pFiles->Init();

	// リング末尾へ移動
	ring_t* p = (ring_t*)((size_t)pFiles - offsetof(ring_t, f));
	p->r.InsertTail(&m_cRing);
}

//===========================================================================
//
//	FCB処理
//
//===========================================================================

//---------------------------------------------------------------------------
//
/// 初期化
//
//---------------------------------------------------------------------------
void CHostFcb::Init()
{
	m_bUpdate = FALSE;
	m_pFile = NULL;
}

//---------------------------------------------------------------------------
//
/// ファイルオープンモードを設定
//
//---------------------------------------------------------------------------
BOOL CHostFcb::SetMode(DWORD nHumanMode)
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
			return FALSE;
	}

	m_bFlag = (nHumanMode & Human68k::OP_SPECIAL) != 0;

	return TRUE;
}

//---------------------------------------------------------------------------
//
/// ファイル名を設定
//
//---------------------------------------------------------------------------
void CHostFcb::SetFilename(const TCHAR* szFilename)
{
	ASSERT(szFilename);
	ASSERT(strlen(szFilename) < FILEPATH_MAX);

	strcpy(m_szFilename, szFilename);
}

//---------------------------------------------------------------------------
//
/// Human68kパス名を設定
//
//---------------------------------------------------------------------------
void CHostFcb::SetHumanPath(const BYTE* szHumanPath)
{
	ASSERT(szHumanPath);
	ASSERT(strlen((const char*)szHumanPath) < HUMAN68K_PATH_MAX);

	strcpy((char*)m_szHumanPath, (const char*)szHumanPath);
}

//---------------------------------------------------------------------------
//
/// ファイル作成
///
/// エラーの時はFALSEを返す。
//
//---------------------------------------------------------------------------
BOOL CHostFcb::Create(Human68k::fcb_t* pFcb, DWORD nHumanAttribute, BOOL bForce)
{
	ASSERT((nHumanAttribute & (Human68k::AT_DIRECTORY | Human68k::AT_VOLUME)) == 0);
	ASSERT(strlen(m_szFilename) > 0);
	ASSERT(m_pFile == NULL);

	// 重複チェック
	if (bForce == FALSE) {
		struct stat sb;
		if (stat(S2U(m_szFilename), &sb) == 0)
			return FALSE;
	}

	// ファイル作成
	m_pFile = fopen(S2U(m_szFilename), "w+b");	/// @warning 理想動作は属性ごと上書き

	return m_pFile != NULL;
}

//---------------------------------------------------------------------------
//
/// ファイルオープン
///
/// エラーの時はFALSEを返す。
//
//---------------------------------------------------------------------------
BOOL CHostFcb::Open()
{
	struct stat st;

	ASSERT(strlen(m_szFilename) > 0);

	// ディレクトリなら失敗
	if (stat(S2U(m_szFilename), &st) == 0) {
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			return FALSE || m_bFlag;
		}
	}

	// ファイルオープン
	if (m_pFile == NULL)
		m_pFile = fopen(S2U(m_szFilename), m_pszMode);

	return m_pFile != NULL || m_bFlag;
}

//---------------------------------------------------------------------------
//
/// ファイルシーク
///
/// エラーの時はFALSEを返す。
//
//---------------------------------------------------------------------------
BOOL CHostFcb::Rewind(DWORD nOffset)
{
	ASSERT(m_pFile);

	if (fseek(m_pFile, nOffset, SEEK_SET))
		return FALSE;

	return ftell(m_pFile) != -1L;
}

//---------------------------------------------------------------------------
//
/// ファイル読み込み
///
/// 0バイト読み込みでも正常動作とする。
/// エラーの時は-1を返す。
//
//---------------------------------------------------------------------------
DWORD CHostFcb::Read(BYTE* pBuffer, DWORD nSize)
{
	ASSERT(pBuffer);
	ASSERT(m_pFile);

	size_t nResult = fread(pBuffer, sizeof(BYTE), nSize, m_pFile);
	if (ferror(m_pFile))
		nResult = (size_t)-1;

	return (DWORD)nResult;
}

//---------------------------------------------------------------------------
//
/// ファイル書き込み
///
/// 0バイト書き込みでも正常動作とする。
/// エラーの時は-1を返す。
//
//---------------------------------------------------------------------------
DWORD CHostFcb::Write(const BYTE* pBuffer, DWORD nSize)
{
	ASSERT(pBuffer);
	ASSERT(m_pFile);

	size_t nResult = fwrite(pBuffer, sizeof(BYTE), nSize, m_pFile);
	if (ferror(m_pFile))
		nResult = (size_t)-1;

	return (DWORD)nResult;
}

//---------------------------------------------------------------------------
//
/// ファイル切り詰め
///
/// エラーの時はFALSEを返す。
//
//---------------------------------------------------------------------------
BOOL CHostFcb::Truncate()
{
	ASSERT(m_pFile);

	return ftruncate(fileno(m_pFile), ftell(m_pFile)) == 0;
}

//---------------------------------------------------------------------------
//
/// ファイルシーク
///
/// エラーの時は-1を返す。
//
//---------------------------------------------------------------------------
DWORD CHostFcb::Seek(DWORD nOffset, DWORD nHumanSeek)
{
	ASSERT(nHumanSeek == Human68k::SK_BEGIN ||
		nHumanSeek == Human68k::SK_CURRENT || nHumanSeek == Human68k::SK_END);
	ASSERT(m_pFile);

	int nSeek;
	switch (nHumanSeek) {
		case Human68k::SK_BEGIN:
			nSeek = SEEK_SET;
			break;
		case Human68k::SK_CURRENT:
			nSeek = SEEK_CUR;
			break;
			// case SK_END:
		default:
			nSeek = SEEK_END;
			break;
	}
	if (fseek(m_pFile, nOffset, nSeek))
		return (DWORD)-1;

	return (DWORD)ftell(m_pFile);
}

//---------------------------------------------------------------------------
//
/// ファイル時刻設定
///
/// エラーの時はFALSEを返す。
//
//---------------------------------------------------------------------------
BOOL CHostFcb::TimeStamp(DWORD nHumanTime)
{
	ASSERT(m_pFile || m_bFlag);

	struct tm t = { 0 };
	t.tm_year = (nHumanTime >> 25) + 80;
	t.tm_mon = ((nHumanTime >> 21) - 1) & 15;
	t.tm_mday = (nHumanTime >> 16) & 31;
	t.tm_hour = (nHumanTime >> 11) & 31;
	t.tm_min = (nHumanTime >> 5) & 63;
	t.tm_sec = (nHumanTime & 31) << 1;
	time_t ti = mktime(&t);
	if (ti == (time_t)-1)
		return FALSE;
	struct utimbuf ut;
	ut.actime = ti;
	ut.modtime = ti;

	// クローズ時に更新時刻が上書きされるのを防止するため
	// タイムスタンプの更新前にフラッシュして同期させる
	fflush(m_pFile);

	return utime(S2U(m_szFilename), &ut) == 0 || m_bFlag;
}

//---------------------------------------------------------------------------
//
/// ファイルクローズ
///
/// エラーの時はFALSEを返す。
//
//---------------------------------------------------------------------------
BOOL CHostFcb::Close()
{
	BOOL bResult = TRUE;

	// ファイルクローズ
	// Close→Free(内部で再度Close)という流れもあるので必ず初期化すること。
	if (m_pFile) {
		fclose(m_pFile);
		m_pFile = NULL;
	}

	return bResult;
}

//===========================================================================
//
// FCB processing manager
//
//===========================================================================

#ifdef _DEBUG
//---------------------------------------------------------------------------
//
// Final destructor
//
//---------------------------------------------------------------------------
CHostFcbManager::~CHostFcbManager()
{
	// Confirm that the entity does not exist (just in case)
	ASSERT(m_cRing.Next() == &m_cRing);
	ASSERT(m_cRing.Prev() == &m_cRing);
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
	ASSERT(m_cRing.Next() == &m_cRing);
	ASSERT(m_cRing.Prev() == &m_cRing);

	// Memory allocation
	for (DWORD i = 0; i < XM6_HOST_FCB_MAX; i++) {
		ring_t* p = new ring_t;
		ASSERT(p);
		p->r.Insert(&m_cRing);
	}
}

//---------------------------------------------------------------------------
//
// Clean (at startup/reset)
//
//---------------------------------------------------------------------------
void CHostFcbManager::Clean()
{

	//  Fast task killer
	CRing* p;
	while ((p = m_cRing.Next()) != &m_cRing) {
		delete (ring_t*)p;
	}
}

//---------------------------------------------------------------------------
//
// Alloc
//
//---------------------------------------------------------------------------
CHostFcb* CHostFcbManager::Alloc(DWORD nKey)
{
	ASSERT(nKey);

	// Select from the end
	ring_t* p = (ring_t*)m_cRing.Prev();

	// Error if in use (just in case)
	if (p->f.isSameKey(0) == FALSE) {
		ASSERT(0);
		return NULL;
	}

	// Move to the top of the ring
	p->r.Insert(&m_cRing);

	//  Set key
	p->f.SetKey(nKey);

	return &p->f;
}

//---------------------------------------------------------------------------
//
// Search
//
//---------------------------------------------------------------------------
CHostFcb* CHostFcbManager::Search(DWORD nKey)
{
	ASSERT(nKey);

	// Search for applicable objects
	ring_t* p = (ring_t*)m_cRing.Next();
	while (p != (ring_t*)&m_cRing) {
		if (p->f.isSameKey(nKey)) {
			 // Move to the top of the ring
			p->r.Insert(&m_cRing);
			return &p->f;
		}
		p = (ring_t*)p->r.Next();
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
// Free
//
//---------------------------------------------------------------------------
void CHostFcbManager::Free(CHostFcb* pFcb)
{
	ASSERT(pFcb);

	// Free
	pFcb->SetKey(0);
	pFcb->Close();

	// Move to the end of the ring
	ring_t* p = (ring_t*)((size_t)pFcb - offsetof(ring_t, f));
	p->r.InsertTail(&m_cRing);
}

//===========================================================================
//
// Host side file system
//
//===========================================================================

DWORD CFileSys::g_nOption;		// File name conversion flag

//---------------------------------------------------------------------------
//
// Default constructor
//
//---------------------------------------------------------------------------
CFileSys::CFileSys()
{
	m_nHostSectorCount = 0;

	// Config data initialization
	m_nDrives = 0;

	for (size_t n = 0; n < DriveMax; n++) {
		m_nFlag[n] = 0;
		m_szBase[n][0] = _T('\0');
	}

	// TwentyOneオプション監視初期化
	m_nKernel = 0;
	m_nKernelSearch = 0;

	// 動作フラグ初期化
	m_nOptionDefault = 0;
	m_nOption = 0;
	ASSERT(g_nOption == 0);

	// 登録したドライブ数は0
	m_nUnits = 0;
}

//---------------------------------------------------------------------------
//
/// リセット (全クローズ)
//
//---------------------------------------------------------------------------
void CFileSys::Reset()
{

	// 仮想セクタ領域初期化
	m_nHostSectorCount = 0;
	memset(m_nHostSectorBuffer, 0, sizeof(m_nHostSectorBuffer));

	// ファイル検索領域 解放 (起動・リセット時)
	m_cFiles.Clean();

	// FCB操作領域 解放 (起動・リセット時)
	m_cFcb.Clean();

	// ディレクトリエントリ 解放 (起動・リセット時)
	m_cEntry.Clean();

	// TwentyOneオプション監視初期化
	m_nKernel = 0;
	m_nKernelSearch = 0;

	// 動作フラグ初期化
	SetOption(m_nOptionDefault);
}

//---------------------------------------------------------------------------
//
/// 初期化 (デバイス起動とロード)
//
//---------------------------------------------------------------------------
void CFileSys::Init()
{

	// ファイル検索領域 初期化 (デバイス起動・ロード時)
	m_cFiles.Init();

	// FCB操作領域 初期化 (デバイス起動・ロード時)
	m_cFcb.Init();

	// ディレクトリエントリ 初期化 (デバイス起動・ロード時)
	m_cEntry.Init();

	// パス個別設定の有無を判定
	DWORD nDrives = m_nDrives;
	if (nDrives == 0) {
		// 個別設定を使わずにルートディレクトリを使用する
		strcpy(m_szBase[0], _T("/"));
		m_nFlag[0] = 0;
		nDrives++;
	}

	// ファイルシステムを登録
	DWORD nUnit = 0;
	for (DWORD n = 0; n < nDrives; n++) {
		// ベースパスが存在しないエントリは登録しない
		if (m_szBase[n][0] == _T('\0'))
			continue;

		// ファイルシステムを1ユニット生成
		CHostDrv* p = new CHostDrv;	// std::nothrow
		if (p) {
			m_cEntry.SetDrv(nUnit, p);
			p->Init(m_szBase[n], m_nFlag[n]);

			// 次のユニットへ
			nUnit++;
		}
	}

	// 登録したドライブ数を保存
	m_nUnits = nUnit;
}

//---------------------------------------------------------------------------
//
/// $40 - デバイス起動
//
//---------------------------------------------------------------------------
DWORD CFileSys::InitDevice(const Human68k::argument_t* pArgument)
{

	// オプション初期化
	InitOption(pArgument);

	// ファイルシステム初期化
	Init();

	return m_nUnits;
}

//---------------------------------------------------------------------------
//
/// $41 - ディレクトリチェック
//
//---------------------------------------------------------------------------
int CFileSys::CheckDir(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(pNamests);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// レジューム後に無効なドライブでmint操作時に白帯を出さないよう改良

	// パス名生成
	CHostFiles f;
	f.SetPath(pNamests);
	if (f.isRootPath())
		return 0;
	f.SetPathOnly();
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_DIRNOTFND;

	return 0;
}

//---------------------------------------------------------------------------
//
/// $42 - ディレクトリ作成
//
//---------------------------------------------------------------------------
int CFileSys::MakeDir(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(pNamests);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// 念のため

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// パス名生成
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetPathOnly();
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_INVALIDPATH;
	f.AddFilename();

	// ディレクトリ作成
	if (mkdir(S2U(f.GetPath()), 0777))
		return FS_INVALIDPATH;

	// キャッシュ更新
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $43 - ディレクトリ削除
//
//---------------------------------------------------------------------------
int CFileSys::RemoveDir(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(pNamests);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// 念のため

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// パス名生成
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetAttribute(Human68k::AT_DIRECTORY);
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_DIRNOTFND;

	// キャッシュ削除
	BYTE szHuman[HUMAN68K_PATH_MAX + 24];
	ASSERT(strlen((const char*)f.GetHumanPath()) +
		strlen((const char*)f.GetHumanFilename()) < HUMAN68K_PATH_MAX + 24);
	strcpy((char*)szHuman, (const char*)f.GetHumanPath());
	strcat((char*)szHuman, (const char*)f.GetHumanFilename());
	strcat((char*)szHuman, "/");
	m_cEntry.DeleteCache(nUnit, szHuman);

	// ディレクトリ削除
	if (rmdir(S2U(f.GetPath())))
		return FS_CANTDELETE;

	// キャッシュ更新
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $44 - ファイル名変更
//
//---------------------------------------------------------------------------
int CFileSys::Rename(DWORD nUnit, const Human68k::namests_t* pNamests, const Human68k::namests_t* pNamestsNew)
{
	ASSERT(pNamests);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// 念のため

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// パス名生成
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_FILENOTFND;

	CHostFiles fNew;
	fNew.SetPath(pNamestsNew);
	fNew.SetPathOnly();
	if (fNew.Find(nUnit, &m_cEntry) == FALSE)
		return FS_INVALIDPATH;
	fNew.AddFilename();

	// キャッシュ更新
	if (f.GetAttribute() & Human68k::AT_DIRECTORY)
		m_cEntry.CleanCacheChild(nUnit, f.GetHumanPath());

	// ファイル名変更
	char szFrom[FILENAME_MAX];
	char szTo[FILENAME_MAX];
	SJIS2UTF8(f.GetPath(), szFrom, FILENAME_MAX);
	SJIS2UTF8(fNew.GetPath(), szTo, FILENAME_MAX);
	if (rename(szFrom, szTo)) {
		return FS_FILENOTFND;
	}

	// キャッシュ更新
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());
	m_cEntry.CleanCache(nUnit, fNew.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $45 - ファイル削除
//
//---------------------------------------------------------------------------
int CFileSys::Delete(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(pNamests);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// 念のため

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// パス名生成
	CHostFiles f;
	f.SetPath(pNamests);
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_FILENOTFND;

	// ファイル削除
	if (unlink(S2U(f.GetPath())))
		return FS_CANTDELETE;

	// キャッシュ更新
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $46 - ファイル属性取得/設定
//
//---------------------------------------------------------------------------
int CFileSys::Attribute(DWORD nUnit, const Human68k::namests_t* pNamests, DWORD nHumanAttribute)
{
	ASSERT(pNamests);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// レジューム後に無効なドライブ上で発生

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// パス名生成
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_FILENOTFND;

	// 属性取得なら終了
	if (nHumanAttribute == 0xFF)
		return f.GetAttribute();

	// 属性チェック
	if (nHumanAttribute & Human68k::AT_VOLUME)
		return FS_CANTACCESS;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// 属性生成
	DWORD nAttribute = (nHumanAttribute & Human68k::AT_READONLY) |
		(f.GetAttribute() & ~Human68k::AT_READONLY);
	if (f.GetAttribute() != nAttribute) {
		struct stat sb;
		if (stat(S2U(f.GetPath()), &sb))
			return FS_FILENOTFND;
		mode_t m = sb.st_mode & 0777;
		if (nAttribute & Human68k::AT_READONLY)
			m &= 0555;	// ugo-w
		else
			m |= 0200;	// u+w

		// 属性設定
		if (chmod(S2U(f.GetPath()), m))
			return FS_FILENOTFND;
	}

	// キャッシュ更新
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	// 変更後の属性取得
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_FILENOTFND;

	return f.GetAttribute();
}

//---------------------------------------------------------------------------
//
/// $47 - ファイル検索
//
//---------------------------------------------------------------------------
int CFileSys::Files(DWORD nUnit, DWORD nKey, const Human68k::namests_t* pNamests, Human68k::files_t* pFiles)
{
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFiles);

	// 既に同じキーを持つ領域があれば解放しておく
	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles != NULL) {
		m_cFiles.Free(pHostFiles);
	}

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// レジューム後に無効なドライブ上で発生

	// ボリュームラベルの取得
	/** @note
	直前のメディア交換チェックで正しくエラーを返しているにもかかわら
	ず、ボリュームラベルの取得を実行する行儀の悪いアプリでホスト側の
	リムーバブルメディア(CD-ROMドライブ等)が白帯を出すのを防ぐため、
	ボリュームラベルの取得はメディアチェックをせずに行なう仕様とした。
	*/
	if ((pFiles->fatr & (Human68k::AT_ARCHIVE | Human68k::AT_DIRECTORY | Human68k::AT_VOLUME))
		== Human68k::AT_VOLUME) {
		// パスチェック
		CHostFiles f;
		f.SetPath(pNamests);
		if (f.isRootPath() == FALSE)
			return FS_FILENOTFND;

		// バッファを確保せず、いきなり結果を返す
		if (FilesVolume(nUnit, pFiles) == FALSE)
			return FS_FILENOTFND;
		return 0;
	}

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// バッファ確保
	pHostFiles = m_cFiles.Alloc(nKey);
	if (pHostFiles == NULL)
		return FS_OUTOFMEM;

	// ディレクトリチェック
	pHostFiles->SetPath(pNamests);
	if (pHostFiles->isRootPath() == FALSE) {
		pHostFiles->SetPathOnly();
		if (pHostFiles->Find(nUnit, &m_cEntry) == FALSE) {
			m_cFiles.Free(pHostFiles);
			return FS_DIRNOTFND;
		}
	}

	// ワイルドカード使用可能に設定
	pHostFiles->SetPathWildcard();
	pHostFiles->SetAttribute(pFiles->fatr);

	// ファイル検索
	if (pHostFiles->Find(nUnit, &m_cEntry) == FALSE) {
		m_cFiles.Free(pHostFiles);
		return FS_FILENOTFND;
	}

	// 検索結果を格納
	pFiles->attr = (BYTE)pHostFiles->GetAttribute();
	pFiles->date = pHostFiles->GetDate();
	pFiles->time = pHostFiles->GetTime();
	pFiles->size = pHostFiles->GetSize();
	strcpy((char*)pFiles->full, (const char*)pHostFiles->GetHumanResult());

	// 擬似ディレクトリエントリを指定
	pFiles->sector = nKey;
	pFiles->offset = 0;

	// ファイル名にワイルドカードがなければ、この時点でバッファを解放可能
	if (pNamests->wildcard == 0) {
		// しかし、仮想セクタのエミュレーションで使う可能性があるため、すぐには解放しない
		// m_cFiles.Free(pHostFiles);
	}

	return 0;
}

//---------------------------------------------------------------------------
//
/// $48 - ファイル次検索
//
//---------------------------------------------------------------------------
int CFileSys::NFiles(DWORD nUnit, DWORD nKey, Human68k::files_t* pFiles)
{
	ASSERT(nKey);
	ASSERT(pFiles);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// レジューム後に無効なドライブ上で発生

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// バッファ検索
	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles == NULL)
		return FS_INVALIDPTR;

	// ファイル検索
	if (pHostFiles->Find(nUnit, &m_cEntry) == FALSE) {
		m_cFiles.Free(pHostFiles);
		return FS_FILENOTFND;
	}

	ASSERT(pFiles->sector == nKey);
	ASSERT(pFiles->offset == 0);

	// 検索結果を格納
	pFiles->attr = (BYTE)pHostFiles->GetAttribute();
	pFiles->date = pHostFiles->GetDate();
	pFiles->time = pHostFiles->GetTime();
	pFiles->size = pHostFiles->GetSize();
	strcpy((char*)pFiles->full, (const char*)pHostFiles->GetHumanResult());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $49 - ファイル新規作成
//
//---------------------------------------------------------------------------
int CFileSys::Create(DWORD nUnit, DWORD nKey, const Human68k::namests_t* pNamests, Human68k::fcb_t* pFcb, DWORD nHumanAttribute, BOOL bForce)
{
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFcb);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// 念のため

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// 既に同じキーを持つ領域があればエラーとする
	if (m_cFcb.Search(nKey) != NULL)
		return FS_INVALIDPTR;

	// パス名生成
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetPathOnly();
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_INVALIDPATH;
	f.AddFilename();

	// 属性チェック
	if (nHumanAttribute & (Human68k::AT_DIRECTORY | Human68k::AT_VOLUME))
		return FS_CANTACCESS;

	// パス名保存
	CHostFcb* pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == NULL)
		return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());
	pHostFcb->SetHumanPath(f.GetHumanPath());

	// オープンモード設定
	pFcb->mode = (WORD)((pFcb->mode & ~Human68k::OP_MASK) | Human68k::OP_FULL);
	if (pHostFcb->SetMode(pFcb->mode) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

	// ファイル作成
	if (pHostFcb->Create(pFcb, nHumanAttribute, bForce) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_FILEEXIST;
	}

	// キャッシュ更新
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $4A - ファイルオープン
//
//---------------------------------------------------------------------------
int CFileSys::Open(DWORD nUnit, DWORD nKey, const Human68k::namests_t* pNamests, Human68k::fcb_t* pFcb)
{
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFcb);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// レジューム後に無効なドライブ上で発生

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	switch (pFcb->mode & Human68k::OP_MASK) {
		case Human68k::OP_WRITE:
		case Human68k::OP_FULL:
			if (m_cEntry.isWriteProtect(nUnit))
				return FS_FATAL_WRITEPROTECT;
	}

	// 既に同じキーを持つ領域があればエラーとする
	if (m_cFcb.Search(nKey) != NULL)
		return FS_INVALIDPRM;

	// パス名生成
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_FILENOTFND;

	// タイムスタンプ
	pFcb->date = f.GetDate();
	pFcb->time = f.GetTime();

	// ファイルサイズ
	pFcb->size = f.GetSize();

	// パス名保存
	CHostFcb* pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == NULL)
		return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());
	pHostFcb->SetHumanPath(f.GetHumanPath());

	// オープンモード設定
	if (pHostFcb->SetMode(pFcb->mode) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

	// ファイルオープン
	if (pHostFcb->Open() == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPATH;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
/// $4B - ファイルクローズ
//
//---------------------------------------------------------------------------
int CFileSys::Close(DWORD nUnit, DWORD nKey, Human68k::fcb_t* /* pFcb */)
{
	ASSERT(nKey);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// レジューム後に無効なドライブ上で発生

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL)
		return FS_INVALIDPRM;

	// ファイルクローズと領域解放
	m_cFcb.Free(pHostFcb);

	// キャッシュ更新
	if (pHostFcb->isUpdate())
		m_cEntry.CleanCache(nUnit, pHostFcb->GetHumanPath());

	/// @note クローズ時のFCBの状態を他のデバイスと合わせたい

	return 0;
}

//---------------------------------------------------------------------------
//
/// $4C - ファイル読み込み
///
/// 0バイト読み込みでも正常終了する。
//
//---------------------------------------------------------------------------
int CFileSys::Read(DWORD nKey, Human68k::fcb_t* pFcb, BYTE* pBuffer, DWORD nSize)
{
	ASSERT(nKey);
	ASSERT(pFcb);
	// ASSERT(pBuffer);			// 必要時のみ判定
	ASSERT(nSize <= 0xFFFFFF);	// クリップ済

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL)
		return FS_NOTOPENED;

	// バッファ存在確認
	if (pBuffer == NULL) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDFUNC;
	}

	// 読み込み
	DWORD nResult;
	nResult = pHostFcb->Read(pBuffer, nSize);
	if (nResult == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDFUNC;	/// @note これに加えてエラーコード10(読み込みエラー)を返すべき
	}
	ASSERT(nResult <= nSize);

	// ファイルポインタ更新
	pFcb->fileptr += nResult;	/// @note オーバーフロー確認は必要じゃろうか？

	return nResult;
}

//---------------------------------------------------------------------------
//
/// $4D - ファイル書き込み
///
/// 0バイト書き込みの場合はファイルを切り詰める。
//
//---------------------------------------------------------------------------
int CFileSys::Write(DWORD nKey, Human68k::fcb_t* pFcb, const BYTE* pBuffer, DWORD nSize)
{
	ASSERT(nKey);
	ASSERT(pFcb);
	// ASSERT(pBuffer);			// 必要時のみ判定
	ASSERT(nSize <= 0xFFFFFF);	// クリップ済

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL)
		return FS_NOTOPENED;

	DWORD nResult;
	if (nSize == 0) {
		// 切り詰め
		if (pHostFcb->Truncate() == FALSE) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTSEEK;
		}

		// ファイルサイズ更新
		pFcb->size = pFcb->fileptr;

		nResult = 0;
	} else {
		// バッファ存在確認
		if (pBuffer == NULL) {
			m_cFcb.Free(pHostFcb);
			return FS_INVALIDFUNC;
		}

		// 書き込み
		nResult = pHostFcb->Write(pBuffer, nSize);
		if (nResult == (DWORD)-1) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTWRITE;	/// @note これに加えてエラーコード11(書き込みエラー)を返すべき
		}
		ASSERT(nResult <= nSize);

		// ファイルポインタ更新
		pFcb->fileptr += nResult;	/// @note オーバーフロー確認は必要じゃろうか？

		// ファイルサイズ更新
		if (pFcb->size < pFcb->fileptr)
			pFcb->size = pFcb->fileptr;
	}

	// フラグ更新
	pHostFcb->SetUpdate();

	return nResult;
}

//---------------------------------------------------------------------------
//
/// $4E - ファイルシーク
//
//---------------------------------------------------------------------------
int CFileSys::Seek(DWORD nKey, Human68k::fcb_t* pFcb, DWORD nSeek, int nOffset)
{
	ASSERT(pFcb);

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL)
		return FS_NOTOPENED;

	// パラメータチェック
	if (nSeek > Human68k::SK_END) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPRM;
	}

	// ファイルシーク
	DWORD nResult = pHostFcb->Seek(nOffset, nSeek);
	if (nResult == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}

	// ファイルポインタ更新
	pFcb->fileptr = nResult;

	return nResult;
}

//---------------------------------------------------------------------------
//
/// $4F - ファイル時刻取得/設定
///
/// 結果の上位16Bitが$FFFFだとエラー。
//
//---------------------------------------------------------------------------
DWORD CFileSys::TimeStamp(DWORD nUnit, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nHumanTime)
{
	ASSERT(nKey);
	ASSERT(pFcb);

	// 取得のみ
	if (nHumanTime == 0)
		return ((DWORD)pFcb->date << 16) | pFcb->time;

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// 念のため

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL)
		return FS_NOTOPENED;

	// 時刻設定
	if (pHostFcb->TimeStamp(nHumanTime) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPRM;
	}
	pFcb->date = (WORD)(nHumanTime >> 16);
	pFcb->time = (WORD)nHumanTime;

	// キャッシュ更新
	m_cEntry.CleanCache(nUnit, pHostFcb->GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $50 - 容量取得
//
//---------------------------------------------------------------------------
int CFileSys::GetCapacity(DWORD nUnit, Human68k::capacity_t* pCapacity)
{
	ASSERT(pCapacity);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// 念のため

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 容量取得
	return m_cEntry.GetCapacity(nUnit, pCapacity);
}

//---------------------------------------------------------------------------
//
/// $51 - ドライブ状態検査/制御
//
//---------------------------------------------------------------------------
int CFileSys::CtrlDrive(DWORD nUnit, Human68k::ctrldrive_t* pCtrlDrive)
{
	ASSERT(pCtrlDrive);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// レジューム後に無効なドライブでmint操作時に白帯を出さないよう改良

	switch (pCtrlDrive->status) {
		case 0:		// 状態検査
		case 9:		// 状態検査2
			pCtrlDrive->status = (BYTE)m_cEntry.GetStatus(nUnit);
			return pCtrlDrive->status;

		case 1:		// イジェクト
		case 2:		// イジェクト禁止1 (未実装)
		case 3:		// イジェクト許可1 (未実装)
		case 4:		// メディア未挿入時にLED点滅 (未実装)
		case 5:		// メディア未挿入時にLED消灯 (未実装)
		case 6:		// イジェクト禁止2 (未実装)
		case 7:		// イジェクト許可2 (未実装)
			return 0;

		case 8:		// イジェクト検査
			return 1;
	}

	return FS_INVALIDFUNC;
}

//---------------------------------------------------------------------------
//
/// $52 - DPB取得
///
/// レジューム後にDPBが取得できないとHuman68k内部でドライブが消滅するため、
/// 範囲外のユニットでもとにかく正常系として処理する。
//
//---------------------------------------------------------------------------
int CFileSys::GetDPB(DWORD nUnit, Human68k::dpb_t* pDpb)
{
	ASSERT(pDpb);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;

	Human68k::capacity_t cap;
	BYTE media = Human68k::MEDIA_REMOTE;
	if (nUnit < m_nUnits) {
		media = m_cEntry.GetMediaByte(nUnit);

		// セクタ情報獲得
		if (m_cEntry.GetCapacityCache(nUnit, &cap) == FALSE) {
			// 手動イジェクトだとメディアチェックをすり抜けるためここで捕捉
			if (m_cEntry.isEnable(nUnit) == FALSE)
				goto none;

			// メディアチェック
			if (m_cEntry.isMediaOffline(nUnit))
				goto none;

			// ドライブ状態取得
			m_cEntry.GetCapacity(nUnit, &cap);
		}
	} else {
	none:
		cap.clusters = 4;	// まっったく問題ないッスよ？
		cap.sectors = 64;
		cap.bytes = 512;
	}

	// シフト数計算
	DWORD nSize = 1;
	DWORD nShift = 0;
	for (;;) {
		if (nSize >= cap.sectors)
			break;
		nSize <<= 1;
		nShift++;
	}

	// セクタ番号計算
	//
	// 以下の順に並べる。
	// クラスタ0: 未使用
	// クラスタ1: FAT
	// クラスタ2: ルートディレクトリ
	// クラスタ3: データ領域(擬似セクタ)
	DWORD nFat = cap.sectors;
	DWORD nRoot = cap.sectors * 2;
	DWORD nData = cap.sectors * 3;

	// DPB設定
	pDpb->sector_size = (WORD)cap.bytes;		// 1セクタ当りのバイト数
	pDpb->cluster_size =
		(BYTE)(cap.sectors - 1);				// 1クラスタ当りのセクタ数 - 1
	pDpb->shift = (BYTE)nShift;					// クラスタ→セクタのシフト数
	pDpb->fat_sector = (WORD)nFat;				// FAT の先頭セクタ番号
	pDpb->fat_max = 1;							// FAT 領域の個数
	pDpb->fat_size = (BYTE)cap.sectors;			// FAT の占めるセクタ数(複写分を除く)
	pDpb->file_max =
		(WORD)(cap.sectors * cap.bytes / 0x20);	// ルートディレクトリに入るファイルの個数
	pDpb->data_sector = (WORD)nData;			// データ領域の先頭セクタ番号
	pDpb->cluster_max = (WORD)cap.clusters;		// 総クラスタ数 + 1
	pDpb->root_sector = (WORD)nRoot;			// ルートディレクトリの先頭セクタ番号
	pDpb->media = media;						// メディアバイト

	return 0;
}

//---------------------------------------------------------------------------
//
/// $53 - セクタ読み込み
///
/// セクタは疑似的に構築したものを使用する。
/// バッファサイズは$200バイト固定。
//
//---------------------------------------------------------------------------
int CFileSys::DiskRead(DWORD nUnit, BYTE* pBuffer, DWORD nSector, DWORD nSize)
{
	ASSERT(pBuffer);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// レジューム後に無効なドライブ上で発生

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// セクタ数1以外の場合はエラー
	if (nSize != 1)
		return FS_INVALIDPRM;

	// セクタ情報獲得
	Human68k::capacity_t cap;
	if (m_cEntry.GetCapacityCache(nUnit, &cap) == FALSE) {
		// ドライブ状態取得
		m_cEntry.GetCapacity(nUnit, &cap);
	}

	// 擬似ディレクトリエントリへのアクセス
	CHostFiles* pHostFiles = m_cFiles.Search(nSector);
	if (pHostFiles) {
		// 擬似ディレクトリエントリを生成
		Human68k::dirent_t* dir = (Human68k::dirent_t*)pBuffer;
		memcpy(pBuffer, pHostFiles->GetEntry(), sizeof(*dir));
		memset(pBuffer + sizeof(*dir), 0xE5, 0x200 - sizeof(*dir));

		// 擬似ディレクトリエントリ内にファイル実体を指す擬似セクタ番号を記録
		// なお、lzdsysでは以下の式で読み込みセクタ番号を算出している。
		// (dirent.cluster - 2) * (dpb.cluster_size + 1) + dpb.data_sector
		/// @warning リトルエンディアン専用
		dir->cluster = (WORD)(m_nHostSectorCount + 2);		// 擬似セクタ番号
		m_nHostSectorBuffer[m_nHostSectorCount] = nSector;	// 擬似セクタの指す実体
		m_nHostSectorCount++;
		m_nHostSectorCount %= XM6_HOST_PSEUDO_CLUSTER_MAX;

		return 0;
	}

	// クラスタ番号からセクタ番号を算出
	DWORD n = nSector - (3 * cap.sectors);
	DWORD nMod = 1;
	if (cap.sectors) {
		// メディアが存在しない場合はcap.sectorsが0になるので注意
		nMod = n % cap.sectors;
		n /= cap.sectors;
	}

	// ファイル実体へのアクセス
	if (nMod == 0 && n < XM6_HOST_PSEUDO_CLUSTER_MAX) {
		pHostFiles = m_cFiles.Search(m_nHostSectorBuffer[n]);	// 実体を検索
		if (pHostFiles) {
			// 擬似セクタを生成
			CHostFcb f;
			f.SetFilename(pHostFiles->GetPath());
			f.SetMode(Human68k::OP_READ);
			if (f.Open() == FALSE)
				return FS_INVALIDPRM;
			memset(pBuffer, 0, 0x200);
			DWORD nResult = f.Read(pBuffer, 0x200);
			f.Close();
			if (nResult == (DWORD)-1)
				return FS_INVALIDPRM;

			return 0;
		}
	}

	return FS_INVALIDPRM;
}

//---------------------------------------------------------------------------
//
/// $54 - セクタ書き込み
//
//---------------------------------------------------------------------------
int CFileSys::DiskWrite(DWORD nUnit)
{

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// 念のため

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// 現実を突きつける
	return FS_INVALIDPRM;
}

//---------------------------------------------------------------------------
//
/// $55 - IOCTRL
//
//---------------------------------------------------------------------------
int CFileSys::Ioctrl(DWORD nUnit, DWORD nFunction, Human68k::ioctrl_t* pIoctrl)
{
	ASSERT(pIoctrl);

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// レジューム後に無効なドライブでmint操作時に白帯を出さないよう改良

	switch (nFunction) {
		case 0:
			// メディアバイトの獲得
			pIoctrl->media = m_cEntry.GetMediaByte(nUnit);
			return 0;

		case 1:
			// Human68k互換のためのダミー
			pIoctrl->param = -1;
			return 0;

		case 2:
			switch (pIoctrl->param) {
				case (DWORD)-1:
					// メディア再認識
					m_cEntry.isMediaOffline(nUnit);
					return 0;

				case 0:
				case 1:
					// Human68k互換のためのダミー
					return 0;
			}
			break;

		case (DWORD)-1:
			// 常駐判定
			memcpy(pIoctrl->buffer, "WindrvXM", 8);
			return 0;

		case (DWORD)-2:
			// オプション設定
			SetOption(pIoctrl->param);
			return 0;

		case (DWORD)-3:
			// オプション獲得
			pIoctrl->param = GetOption();
			return 0;
	}

	return FS_NOTIOCTRL;
}

//---------------------------------------------------------------------------
//
/// $56 - フラッシュ
//
//---------------------------------------------------------------------------
int CFileSys::Flush(DWORD nUnit)
{

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// レジューム後に無効なドライブでmintからコマンドを実行し戻る時に白帯を出さないよう改良

	// 常に成功
	return 0;
}

//---------------------------------------------------------------------------
//
/// $57 - メディア交換チェック
//
//---------------------------------------------------------------------------
int CFileSys::CheckMedia(DWORD nUnit)
{

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// レジューム後に無効なドライブでmint操作時に白帯を出さないよう改良

	// メディア交換チェック
	BOOL bResult = m_cEntry.CheckMedia(nUnit);

	// メディア未挿入ならエラーとする
	if (bResult == FALSE) {
		return FS_INVALIDFUNC;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
/// $58 - 排他制御
//
//---------------------------------------------------------------------------
int CFileSys::Lock(DWORD nUnit)
{

	// ユニットチェック
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// 念のため

	// メディアチェック
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// 常に成功
	return 0;
}

//---------------------------------------------------------------------------
//
/// オプション設定
//
//---------------------------------------------------------------------------
void CFileSys::SetOption(DWORD nOption)
{

	// オプション設定変更でキャッシュクリア
	if (m_nOption ^ nOption)
		m_cEntry.CleanCache();

	m_nOption = nOption;
	g_nOption = nOption;
}

//---------------------------------------------------------------------------
//
/// オプション初期化
//
//---------------------------------------------------------------------------
void CFileSys::InitOption(const Human68k::argument_t* pArgument)
{
	ASSERT(pArgument);

	// ドライブ数を初期化
	m_nDrives = 0;

	const BYTE* pp = pArgument->buf;
	pp += strlen((const char*)pp) + 1;

	DWORD nOption = m_nOptionDefault;
	for (;;) {
		ASSERT(pp < pArgument->buf + sizeof(*pArgument));
		const BYTE* p = pp;
		BYTE c = *p++;
		if (c == '\0')
			break;

		DWORD nMode;
		if (c == '+') {
			nMode = 1;
		} else if (c == '-') {
			nMode = 0;
		} else if (c == '/') {
			// デフォルトベースパスの指定
			if (m_nDrives < DriveMax) {
				p--;
				strcpy(m_szBase[m_nDrives], (const char *)p);
				m_nDrives++;
			}
			pp += strlen((const char*)pp) + 1;
			continue;
		} else {
			// オプション指定ではないので次へ
			pp += strlen((const char*)pp) + 1;
			continue;
		}

		for (;;) {
			c = *p++;
			if (c == '\0')
				break;

			DWORD nBit = 0;
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
			}

			if (nMode)
				nOption |= nBit;
			else
				nOption &= ~nBit;
		}

		pp = p;
	}

	// オプション設定
	if (nOption != m_nOption) {
		SetOption(nOption);
	}
}

//---------------------------------------------------------------------------
//
/// ボリュームラベル取得
//
//---------------------------------------------------------------------------
BOOL CFileSys::FilesVolume(DWORD nUnit, Human68k::files_t* pFiles)
{
	ASSERT(pFiles);

	// ボリュームラベル取得
	TCHAR szVolume[32];
	BOOL bResult = m_cEntry.GetVolumeCache(nUnit, szVolume);
	if (bResult == FALSE) {
		// 手動イジェクトだとメディアチェックをすり抜けるためここで捕捉
		if (m_cEntry.isEnable(nUnit) == FALSE)
			return FALSE;

		// メディアチェック
		if (m_cEntry.isMediaOffline(nUnit))
			return FALSE;

		// ボリュームラベル取得
		m_cEntry.GetVolume(nUnit, szVolume);
	}
	if (szVolume[0] == _T('\0'))
		return FALSE;

	pFiles->attr = Human68k::AT_VOLUME;
	pFiles->time = 0;
	pFiles->date = 0;
	pFiles->size = 0;

	CHostFilename fname;
	fname.SetHost(szVolume);
	fname.ConvertHuman();
	strcpy((char*)pFiles->full, (const char*)fname.GetHuman());

	return TRUE;
}
