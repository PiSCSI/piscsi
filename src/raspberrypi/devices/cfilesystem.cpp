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
		if (((TCHAR)0x80 <= c && c <= (TCHAR)0x9F) || (TCHAR)0xE0 <= c) {	// To be precise: 0x81~0x9F 0xE0~0xEF
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

	// Find something that matches perfectly with either of the stored file names
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

	// Find in cache
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

		// Find the next path
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
/// Find host side name (path name + file name (can be abbeviated) + attribute)
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

	// Find file name
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
/// Compare arrays (supports wildcards)
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
/// Compare Human68k side name
//
//---------------------------------------------------------------------------
BOOL CHostPath::isSameHuman(const BYTE* szHuman) const
{
	ASSERT(szHuman);

	// Calulate number of chars
	size_t nLength = strlen((const char*)m_szHuman);
	size_t n = strlen((const char*)szHuman);

	// Check number of chars
	if (nLength != n)
		return FALSE;

	// Compare Human68k path name
	return Compare(m_szHuman, m_szHuman + nLength, szHuman, szHuman + n) == 0;
}

BOOL CHostPath::isSameChild(const BYTE* szHuman) const
{
	ASSERT(szHuman);

	// Calulate number of chars
	size_t nLength = strlen((const char*)m_szHuman);
	size_t n = strlen((const char*)szHuman);

	// Check number of chars
	if (nLength < n)
		return FALSE;

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
const CHostFilename* CHostPath::FindFilename(const BYTE* szHuman, DWORD nHumanAttribute) const
{
	ASSERT(szHuman);

	// Calulate number of chars
	const BYTE* pFirst = szHuman;
	size_t nLength = strlen((const char*)pFirst);
	const BYTE* pLast = pFirst + nLength;

	// Find something that matches perfectly with either of the stored file names
	const ring_t* p = (ring_t*)m_cRing.Next();
	for (; p != (ring_t*)&m_cRing; p = (ring_t*)p->r.Next()) {
		if (p->f.CheckAttribute(nHumanAttribute) == 0)
			continue;
		// Calulate number of chars
		const BYTE* pBufFirst = p->f.GetHuman();
		const BYTE* pBufLast = p->f.GetHumanLast();
		size_t nBufLength = pBufLast - pBufFirst;
		// Check number of chars
		if (nLength != nBufLength)
			continue;
		// File name check
		if (Compare(pFirst, pLast, pBufFirst, pBufLast) == 0)
			return &p->f;
	}

	return NULL;
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
const CHostFilename* CHostPath::FindFilenameWildcard(const BYTE* szHuman, DWORD nHumanAttribute, find_t* pFind) const
{
	ASSERT(szHuman);
	ASSERT(pFind);

	// Split the base file name and Human68k file extension
	const BYTE* pFirst = szHuman;
	const BYTE* pLast = pFirst + strlen((const char*)pFirst);
	const BYTE* pExt = CHostFilename::SeparateExt(pFirst);

	// Move to the start position
	const ring_t* p = (ring_t*)m_cRing.Next();
	if (pFind->count > 0) {
		if (pFind->id == m_nId) {
			// If the same directory entry, continue right away from the previous position
			p = pFind->pos;
		} else {
			// Find the start position in the directory entry contents
			DWORD n = 0;
			for (;; p = (ring_t*)p->r.Next()) {
				if (p == (ring_t*)&m_cRing) {
					// Extrapolate from the count when the same entry isn't found (just in case)
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
					// Same entry is found
					pFind->count = n;
					break;
				}
				n++;
			}
		}
	}

	// Find files
	for (; p != (ring_t*)&m_cRing; p = (ring_t*)p->r.Next()) {
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
/// Confirm that the file update has been carried out
//
//---------------------------------------------------------------------------
BOOL CHostPath::isRefresh()
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
	ASSERT(strlen(m_szHost) + 22 < FILEPATH_MAX);

	// Store time stamp
	Backup();

	TCHAR szPath[FILEPATH_MAX];
	strcpy(szPath, m_szHost);

	// Update refresh flag
	m_bRefresh = FALSE;

	// Store previous cache contents
	CRing cRingBackup;
	m_cRing.InsertRing(&cRingBackup);

	// Register file name
	/// TODO: Process file duplication by ourselves rather than using the host API.
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

		// When at the top level directory, exclude current and parent
		struct dirent* pe = pd[i];
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
		ring_t* pCache = (ring_t*)cRingBackup.Next();
		for (;;) {
			if (pCache == (ring_t*)&cRingBackup) {
				pCache = NULL;			// No relevant entry
				bUpdate = TRUE;			// Confirm new entry
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
			for (DWORD n = 0; n < XM6_HOST_FILENAME_PATTERN_MAX; n++) {
				// Confirm file name validity
				if (pFilename->isCorrect()) {
					// Confirm match with previous entry
					const CHostFilename* pCheck = FindFilename(pFilename->GetHuman());
					if (pCheck == NULL) {
						// If no match, confirm existence of real file
						strcpy(szPath, m_szHost);
						strcat(szPath, (const char*)pFilename->GetHuman());
						struct stat sb;
						if (stat(S2U(szPath), &sb))
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

		DWORD nHumanSize = (DWORD)sb.st_size;
		pFilename->SetEntrySize(nHumanSize);

		WORD nHumanDate = 0;
		WORD nHumanTime = 0;
		struct tm* pt = localtime(&sb.st_mtime);
		if (pt) {
			nHumanDate = (WORD)(((pt->tm_year - 80) << 9) | ((pt->tm_mon + 1) << 5) | pt->tm_mday);
			nHumanTime = (WORD)((pt->tm_hour << 11) | (pt->tm_min << 5) | (pt->tm_sec >> 1));
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
				bUpdate = TRUE;			// Flag for update if no match
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
		bUpdate = TRUE;					// Confirms the decrease in entries due to deletion
		Free(p);
	}

	// Update the identifier if the update has been carried out
	if (bUpdate)
		m_nId = ++g_nId;
	//	ASSERT(m_nId);
}

//---------------------------------------------------------------------------
//
/// Store the host side time stamp
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
	if (len > 1) {	// Don't do anything if it is the root directory
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
/// Restore the host side time stamp
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
/// Update
//
//---------------------------------------------------------------------------
void CHostPath::Release()
{

	m_bRefresh = TRUE;
}

//===========================================================================
//
//	Manage directory entries
//
//===========================================================================

CHostEntry::CHostEntry()
{
	for (size_t n = 0; n < DriveMax; n++) {
		m_pDrv[n] = NULL;
	}

	m_nTimeout = 0;
}

CHostEntry::~CHostEntry()
{
	Clean();

#ifdef _DEBUG
	// Confirm object
	for (size_t n = 0; n < DriveMax; n++) {
		ASSERT(m_pDrv[n] == NULL);
	}
#endif	// _DEBUG
}

//---------------------------------------------------------------------------
//
/// Initialize (when the driver is installed)
//
//---------------------------------------------------------------------------
void CHostEntry::Init()
{

#ifdef _DEBUG
	// Confirm object
	for (size_t n = 0; n < DriveMax; n++) {
		ASSERT(m_pDrv[n] == NULL);
	}
#endif	// _DEBUG
}

//---------------------------------------------------------------------------
//
/// Release (at startup and reset)
//
//---------------------------------------------------------------------------
void CHostEntry::Clean()
{

	// Delete object
	for (size_t n = 0; n < DriveMax; n++) {
		delete m_pDrv[n];
		m_pDrv[n] = NULL;
	}
}

//---------------------------------------------------------------------------
//
/// Update all cache
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
/// Update the cache for the specified unit
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
/// Update the cache for the specified path
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
/// Update all cache for the specified path and below
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
/// Delete cache for the specified path
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
/// Find host side names (path name + file name (can be abbreviated) + attribute)
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
/// Drive settings
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
/// Is it write-protected?
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
/// Is it accessible?
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
/// Media check
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
/// Get media byte
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
/// Get drive status
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
/// Media change check
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
/// Eject
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
/// Get volume label
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
/// Get volume label from cache
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
/// Get capacity
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
/// Get cluster size from cache
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
	ASSERT(szHuman);
	ASSERT(szBuffer);

	const size_t nMax = 22;
	const BYTE* p = szHuman;

	BYTE c = *p++;				// Read
	if (c != '/' && c != '\\')
		return NULL;			// Error: Invalid path name

	// Insert one file
	size_t i = 0;
	for (;;) {
		c = *p;					// Read
		if (c == '\0')
			break;				// Exit if at the end of an array (return the end position)
		if (c == '/' || c == '\\') {
			if (i == 0)
				return NULL;	// Error: Two separator chars appear in sequence
			break;				// Exit after reading the separator (return the char position)
		}
		p++;

		if (i >= nMax)
			return NULL;		// Error: The first byte hits the end of the buffer
		szBuffer[i++] = c;		// Read

		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// Specifically 0x81~0x9F and 0xE0~0xEF
			c = *p++;			// Read
			if (c < 0x40)
				return NULL;	// Error: Invalid Shift-JIS 2nd byte

			if (i >= nMax)
				return NULL;	// Error: The second byte hits the end of the buffer
			szBuffer[i++] = c;	// Read
		}
	}
	szBuffer[i] = '\0';			// Read

	return p;
}

//===========================================================================
//
//	File search processing
//
//===========================================================================

void CHostFiles::Init()
{
}

//---------------------------------------------------------------------------
//
/// Generate path and file name internally
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
/// Find file on the Human68k side and create data on the host side
//
//---------------------------------------------------------------------------
BOOL CHostFiles::Find(DWORD nUnit, CHostEntry* pEntry)
{
	ASSERT(pEntry);

	return pEntry->Find(nUnit, this);
}

//---------------------------------------------------------------------------
//
/// Find file name
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
/// Store the Human68k side search results
//
//---------------------------------------------------------------------------
void CHostFiles::SetEntry(const CHostFilename* pFilename)
{
	ASSERT(pFilename);

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
	ASSERT(szPath);
	ASSERT(strlen(szPath) < FILEPATH_MAX);

	strcpy(m_szHostResult, szPath);
}

//---------------------------------------------------------------------------
//
/// Add file name to the host side name
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
/// Add a new Human68k file name to the host side name
//
//---------------------------------------------------------------------------
void CHostFiles::AddFilename()
{
	ASSERT(strlen(m_szHostResult) + strlen((const char*)m_szHumanFilename) < FILEPATH_MAX);
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
	ASSERT(m_cRing.Next() == &m_cRing);
	ASSERT(m_cRing.Prev() == &m_cRing);
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
	ASSERT(m_cRing.Next() == &m_cRing);
	ASSERT(m_cRing.Prev() == &m_cRing);

	// Allocate memory
	for (DWORD i = 0; i < XM6_HOST_FILES_MAX; i++) {
		ring_t* p = new ring_t;
		ASSERT(p);
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

CHostFiles* CHostFilesManager::Alloc(DWORD nKey)
{
	ASSERT(nKey);

	// Select from the end
	ring_t* p = (ring_t*)m_cRing.Prev();

	// Move to the start of the ring
	p->r.Insert(&m_cRing);

	p->f.SetKey(nKey);

	return &p->f;
}

CHostFiles* CHostFilesManager::Search(DWORD nKey)
{
	// ASSERT(nKey);	// The search key may become 0 due to DPB damage

	// Find the relevant object
	ring_t* p = (ring_t*)m_cRing.Next();
	for (; p != (ring_t*)&m_cRing; p = (ring_t*)p->r.Next()) {
		if (p->f.isSameKey(nKey)) {
			// Move to the start of the ring
			p->r.Insert(&m_cRing);
			return &p->f;
		}
	}

	return NULL;
}

void CHostFilesManager::Free(CHostFiles* pFiles)
{
	ASSERT(pFiles);

	// Release
	pFiles->SetKey(0);
	pFiles->Init();

	// Move to the end of the ring
	ring_t* p = (ring_t*)((size_t)pFiles - offsetof(ring_t, f));
	p->r.InsertTail(&m_cRing);
}

//===========================================================================
//
//	FCB processing
//
//===========================================================================

void CHostFcb::Init()
{
	m_bUpdate = FALSE;
	m_pFile = NULL;
}

//---------------------------------------------------------------------------
//
/// Set file open mode
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

void CHostFcb::SetFilename(const TCHAR* szFilename)
{
	ASSERT(szFilename);
	ASSERT(strlen(szFilename) < FILEPATH_MAX);

	strcpy(m_szFilename, szFilename);
}

void CHostFcb::SetHumanPath(const BYTE* szHumanPath)
{
	ASSERT(szHumanPath);
	ASSERT(strlen((const char*)szHumanPath) < HUMAN68K_PATH_MAX);

	strcpy((char*)m_szHumanPath, (const char*)szHumanPath);
}

//---------------------------------------------------------------------------
//
/// Create file
///
/// Return FALSE if error is thrown.
//
//---------------------------------------------------------------------------
BOOL CHostFcb::Create(Human68k::fcb_t* pFcb, DWORD nHumanAttribute, BOOL bForce)
{
	ASSERT((nHumanAttribute & (Human68k::AT_DIRECTORY | Human68k::AT_VOLUME)) == 0);
	ASSERT(strlen(m_szFilename) > 0);
	ASSERT(m_pFile == NULL);

	// Duplication check
	if (bForce == FALSE) {
		struct stat sb;
		if (stat(S2U(m_szFilename), &sb) == 0)
			return FALSE;
	}

	// Create file
	m_pFile = fopen(S2U(m_szFilename), "w+b");	/// @warning The ideal operation is to overwrite each attribute

	return m_pFile != NULL;
}

//---------------------------------------------------------------------------
//
/// File open
///
/// Return FALSE if error is thrown.
//
//---------------------------------------------------------------------------
BOOL CHostFcb::Open()
{
	struct stat st;

	ASSERT(strlen(m_szFilename) > 0);

	// Fail if directory
	if (stat(S2U(m_szFilename), &st) == 0) {
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			return FALSE || m_bFlag;
		}
	}

	// File open
	if (m_pFile == NULL)
		m_pFile = fopen(S2U(m_szFilename), m_pszMode);

	return m_pFile != NULL || m_bFlag;
}

//---------------------------------------------------------------------------
//
/// File seek
///
/// Return FALSE if error is thrown.
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
/// Read file
///
/// Handle a 0 byte read as normal operation too.
/// Return -1 if error is thrown.
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
/// Write file
///
/// Handle a 0 byte read as normal operation too.
/// Return -1 if error is thrown.
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
/// Truncate file
///
/// Return FALSE if error is thrown.
//
//---------------------------------------------------------------------------
BOOL CHostFcb::Truncate()
{
	ASSERT(m_pFile);

	return ftruncate(fileno(m_pFile), ftell(m_pFile)) == 0;
}

//---------------------------------------------------------------------------
//
/// File seek
///
/// Return -1 if error is thrown.
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
/// Set file time stamp
///
/// Return FALSE if error is thrown.
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

	// This is for preventing the last updated time stamp to be overwritten upon closing.
	// Flush and synchronize before updating the time stamp.
	fflush(m_pFile);

	return utime(S2U(m_szFilename), &ut) == 0 || m_bFlag;
}

//---------------------------------------------------------------------------
//
/// File close
///
/// Return FALSE if error is thrown.
//
//---------------------------------------------------------------------------
BOOL CHostFcb::Close()
{
	BOOL bResult = TRUE;

	// File close
	// Always initialize because of the Close→Free (internally one more Close) flow.
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

CFileSys::CFileSys()
{
	m_nHostSectorCount = 0;

	// Config data initialization
	m_nDrives = 0;

	for (size_t n = 0; n < DriveMax; n++) {
		m_nFlag[n] = 0;
		m_szBase[n][0] = _T('\0');
	}

	// Initialize TwentyOne option monitoring
	m_nKernel = 0;
	m_nKernelSearch = 0;

	// Initialize operational flags
	m_nOptionDefault = 0;
	m_nOption = 0;
	ASSERT(g_nOption == 0);

	// Number of registered drives are 0
	m_nUnits = 0;
}

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
	DWORD nDrives = m_nDrives;
	if (nDrives == 0) {
		// Use root directory instead of per-path settings
		strcpy(m_szBase[0], _T("/"));
		m_nFlag[0] = 0;
		nDrives++;
	}

	// Register file system
	DWORD nUnit = 0;
	for (DWORD n = 0; n < nDrives; n++) {
		// Don't register is base path do not exist
		if (m_szBase[n][0] == _T('\0'))
			continue;

		// Create 1 unit file system
		CHostDrv* p = new CHostDrv;	// std::nothrow
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
DWORD CFileSys::InitDevice(const Human68k::argument_t* pArgument)
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
int CFileSys::CheckDir(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(pNamests);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// Avoid triggering a fatal error in mint when resuming with an invalid drive

	// Generate path name
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
/// $42 - Create directory
//
//---------------------------------------------------------------------------
int CFileSys::MakeDir(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(pNamests);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
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
	if (f.Find(nUnit, &m_cEntry) == FALSE)
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
int CFileSys::RemoveDir(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(pNamests);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
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
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_DIRNOTFND;

	// Delete cache
	BYTE szHuman[HUMAN68K_PATH_MAX + 24];
	ASSERT(strlen((const char*)f.GetHumanPath()) +
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
int CFileSys::Rename(DWORD nUnit, const Human68k::namests_t* pNamests, const Human68k::namests_t* pNamestsNew)
{
	ASSERT(pNamests);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
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
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_FILENOTFND;

	CHostFiles fNew;
	fNew.SetPath(pNamestsNew);
	fNew.SetPathOnly();
	if (fNew.Find(nUnit, &m_cEntry) == FALSE)
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
int CFileSys::Delete(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(pNamests);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
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
	if (f.Find(nUnit, &m_cEntry) == FALSE)
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
int CFileSys::Attribute(DWORD nUnit, const Human68k::namests_t* pNamests, DWORD nHumanAttribute)
{
	ASSERT(pNamests);

	// Unit check
	if (nUnit >= DriveMax)
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
	if (f.Find(nUnit, &m_cEntry) == FALSE)
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

		// Set attribute
		if (chmod(S2U(f.GetPath()), m))
			return FS_FILENOTFND;
	}

	// Update cache
	m_cEntry.CleanCache(nUnit, f.GetHumanPath());

	// Get attribute after changing
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_FILENOTFND;

	return f.GetAttribute();
}

//---------------------------------------------------------------------------
//
/// $47 - File search
//
//---------------------------------------------------------------------------
int CFileSys::Files(DWORD nUnit, DWORD nKey, const Human68k::namests_t* pNamests, Human68k::files_t* pFiles)
{
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFiles);

	// Release if memory with the same key already exists
	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles != NULL) {
		m_cFiles.Free(pHostFiles);
	}

	// Unit check
	if (nUnit >= DriveMax)
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
		if (f.isRootPath() == FALSE)
			return FS_FILENOTFND;

		// Immediately return the results without allocating buffer
		if (FilesVolume(nUnit, pFiles) == FALSE)
			return FS_FILENOTFND;
		return 0;
	}

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Allocate buffer
	pHostFiles = m_cFiles.Alloc(nKey);
	if (pHostFiles == NULL)
		return FS_OUTOFMEM;

	// Directory check
	pHostFiles->SetPath(pNamests);
	if (pHostFiles->isRootPath() == FALSE) {
		pHostFiles->SetPathOnly();
		if (pHostFiles->Find(nUnit, &m_cEntry) == FALSE) {
			m_cFiles.Free(pHostFiles);
			return FS_DIRNOTFND;
		}
	}

	// Enable wildcards
	pHostFiles->SetPathWildcard();
	pHostFiles->SetAttribute(pFiles->fatr);

	// Find file
	if (pHostFiles->Find(nUnit, &m_cEntry) == FALSE) {
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

	// When the file name does not include wildcards, the buffer may be released
	if (pNamests->wildcard == 0) {
		// However, there is a chance the virtual selector may be used for emulation, so don't release immediately
		// m_cFiles.Free(pHostFiles);
	}

	return 0;
}

//---------------------------------------------------------------------------
//
/// $48 - Search next file
//
//---------------------------------------------------------------------------
int CFileSys::NFiles(DWORD nUnit, DWORD nKey, Human68k::files_t* pFiles)
{
	ASSERT(nKey);
	ASSERT(pFiles);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// This occurs when resuming with an invalid drive

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Find buffer
	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles == NULL)
		return FS_INVALIDPTR;

	// Find file
	if (pHostFiles->Find(nUnit, &m_cEntry) == FALSE) {
		m_cFiles.Free(pHostFiles);
		return FS_FILENOTFND;
	}

	ASSERT(pFiles->sector == nKey);
	ASSERT(pFiles->offset == 0);

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
int CFileSys::Create(DWORD nUnit, DWORD nKey, const Human68k::namests_t* pNamests, Human68k::fcb_t* pFcb, DWORD nHumanAttribute, BOOL bForce)
{
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFcb);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// Just in case

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Write-protect check
	if (m_cEntry.isWriteProtect(nUnit))
		return FS_FATAL_WRITEPROTECT;

	// Release if memory with the same key already exists
	if (m_cFcb.Search(nKey) != NULL)
		return FS_INVALIDPTR;

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetPathOnly();
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_INVALIDPATH;
	f.AddFilename();

	// Attribute check
	if (nHumanAttribute & (Human68k::AT_DIRECTORY | Human68k::AT_VOLUME))
		return FS_CANTACCESS;

	// Store path name
	CHostFcb* pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == NULL)
		return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());
	pHostFcb->SetHumanPath(f.GetHumanPath());

	// Set open mode
	pFcb->mode = (WORD)((pFcb->mode & ~Human68k::OP_MASK) | Human68k::OP_FULL);
	if (pHostFcb->SetMode(pFcb->mode) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

	// Create file
	if (pHostFcb->Create(pFcb, nHumanAttribute, bForce) == FALSE) {
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
int CFileSys::Open(DWORD nUnit, DWORD nKey, const Human68k::namests_t* pNamests, Human68k::fcb_t* pFcb)
{
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFcb);

	// Unit check
	if (nUnit >= DriveMax)
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
	}

	// Release if memory with the same key already exists
	if (m_cFcb.Search(nKey) != NULL)
		return FS_INVALIDPRM;

	// Generate path name
	CHostFiles f;
	f.SetPath(pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (f.Find(nUnit, &m_cEntry) == FALSE)
		return FS_FILENOTFND;

	// Time stamp
	pFcb->date = f.GetDate();
	pFcb->time = f.GetTime();

	// File size
	pFcb->size = f.GetSize();

	// Store path name
	CHostFcb* pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == NULL)
		return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());
	pHostFcb->SetHumanPath(f.GetHumanPath());

	// Set open mode
	if (pHostFcb->SetMode(pFcb->mode) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

	// File open
	if (pHostFcb->Open() == FALSE) {
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
int CFileSys::Close(DWORD nUnit, DWORD nKey, Human68k::fcb_t* /* pFcb */)
{
	ASSERT(nKey);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_FATAL_MEDIAOFFLINE;	// This occurs when resuming with an invalid drive

	// Media check
	if (m_cEntry.isMediaOffline(nUnit))
		return FS_FATAL_MEDIAOFFLINE;

	// Throw error if memory with the same key does not exist
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL)
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
int CFileSys::Read(DWORD nKey, Human68k::fcb_t* pFcb, BYTE* pBuffer, DWORD nSize)
{
	ASSERT(nKey);
	ASSERT(pFcb);
	// ASSERT(pBuffer);			// Evaluate only when needed
	ASSERT(nSize <= 0xFFFFFF);	// Clipped

	// Throw error if memory with the same key does not exist
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL)
		return FS_NOTOPENED;

	// Confirm the existence of the buffer
	if (pBuffer == NULL) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDFUNC;
	}

	// Read
	DWORD nResult;
	nResult = pHostFcb->Read(pBuffer, nSize);
	if (nResult == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDFUNC;	// TODO: Should return error code 10 (read error) as well here
	}
	ASSERT(nResult <= nSize);

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
int CFileSys::Write(DWORD nKey, Human68k::fcb_t* pFcb, const BYTE* pBuffer, DWORD nSize)
{
	ASSERT(nKey);
	ASSERT(pFcb);
	// ASSERT(pBuffer);			// Evaluate only when needed
	ASSERT(nSize <= 0xFFFFFF);	// Clipped

	// Throw error if memory with the same key does not exist
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL)
		return FS_NOTOPENED;

	DWORD nResult;
	if (nSize == 0) {
		// Truncate
		if (pHostFcb->Truncate() == FALSE) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTSEEK;
		}

		// Update file size
		pFcb->size = pFcb->fileptr;

		nResult = 0;
	} else {
		// Confirm the existence of the buffer
		if (pBuffer == NULL) {
			m_cFcb.Free(pHostFcb);
			return FS_INVALIDFUNC;
		}

		// Write
		nResult = pHostFcb->Write(pBuffer, nSize);
		if (nResult == (DWORD)-1) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTWRITE;	/// TODO: Should return error code 11 (write error) as well here
		}
		ASSERT(nResult <= nSize);

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
int CFileSys::Seek(DWORD nKey, Human68k::fcb_t* pFcb, DWORD nSeek, int nOffset)
{
	ASSERT(pFcb);

	// Throw error if memory with the same key does not exist
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL)
		return FS_NOTOPENED;

	// Parameter check
	if (nSeek > Human68k::SK_END) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPRM;
	}

	// File seek
	DWORD nResult = pHostFcb->Seek(nOffset, nSeek);
	if (nResult == (DWORD)-1) {
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
DWORD CFileSys::TimeStamp(DWORD nUnit, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nHumanTime)
{
	ASSERT(nKey);
	ASSERT(pFcb);

	// Get only
	if (nHumanTime == 0)
		return ((DWORD)pFcb->date << 16) | pFcb->time;

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
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
	if (pHostFcb == NULL)
		return FS_NOTOPENED;

	// Set time stamp
	if (pHostFcb->TimeStamp(nHumanTime) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPRM;
	}
	pFcb->date = (WORD)(nHumanTime >> 16);
	pFcb->time = (WORD)nHumanTime;

	// Update cache
	m_cEntry.CleanCache(nUnit, pHostFcb->GetHumanPath());

	return 0;
}

//---------------------------------------------------------------------------
//
/// $50 - Get capacity
//
//---------------------------------------------------------------------------
int CFileSys::GetCapacity(DWORD nUnit, Human68k::capacity_t* pCapacity)
{
	ASSERT(pCapacity);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
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
int CFileSys::CtrlDrive(DWORD nUnit, Human68k::ctrldrive_t* pCtrlDrive)
{
	ASSERT(pCtrlDrive);

	// Unit check
	if (nUnit >= DriveMax)
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
int CFileSys::GetDPB(DWORD nUnit, Human68k::dpb_t* pDpb)
{
	ASSERT(pDpb);

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;

	Human68k::capacity_t cap;
	BYTE media = Human68k::MEDIA_REMOTE;
	if (nUnit < m_nUnits) {
		media = m_cEntry.GetMediaByte(nUnit);

		// Acquire sector data
		if (m_cEntry.GetCapacityCache(nUnit, &cap) == FALSE) {
			// Carry out an extra media check here because it may be skipped when doing a manual eject
			if (m_cEntry.isEnable(nUnit) == FALSE)
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
	DWORD nSize = 1;
	DWORD nShift = 0;
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
	DWORD nFat = cap.sectors;
	DWORD nRoot = cap.sectors * 2;
	DWORD nData = cap.sectors * 3;

	// Set DPB
	pDpb->sector_size = (WORD)cap.bytes;		// Bytes per sector
	pDpb->cluster_size =
		(BYTE)(cap.sectors - 1);				// Sectors per cluster - 1
	pDpb->shift = (BYTE)nShift;					// Number of cluster → sector shifts
	pDpb->fat_sector = (WORD)nFat;				// First FAT sector number
	pDpb->fat_max = 1;							// Number of FAT memory spaces
	pDpb->fat_size = (BYTE)cap.sectors;			// Number of sectors controlled by FAT (excluding copies)
	pDpb->file_max =
		(WORD)(cap.sectors * cap.bytes / 0x20);	// Number of files in the root directory
	pDpb->data_sector = (WORD)nData;			// First sector number of data memory
	pDpb->cluster_max = (WORD)cap.clusters;		// Total number of clusters + 1
	pDpb->root_sector = (WORD)nRoot;			// First sector number of the root directory
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
int CFileSys::DiskRead(DWORD nUnit, BYTE* pBuffer, DWORD nSector, DWORD nSize)
{
	ASSERT(pBuffer);

	// Unit check
	if (nUnit >= DriveMax)
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
	if (m_cEntry.GetCapacityCache(nUnit, &cap) == FALSE) {
		// Get drive status
		m_cEntry.GetCapacity(nUnit, &cap);
	}

	// Access pseudo-directory entry
	CHostFiles* pHostFiles = m_cFiles.Search(nSector);
	if (pHostFiles) {
		// Generate pseudo-directory entry
		Human68k::dirent_t* dir = (Human68k::dirent_t*)pBuffer;
		memcpy(pBuffer, pHostFiles->GetEntry(), sizeof(*dir));
		memset(pBuffer + sizeof(*dir), 0xE5, 0x200 - sizeof(*dir));

		// Register the pseudo-sector number that points to the file entity inside the pseudo-directory.
		// Note that in lzdsys the sector number to read is calculated by the following formula:
		// (dirent.cluster - 2) * (dpb.cluster_size + 1) + dpb.data_sector
		/// @warning little endian only
		dir->cluster = (WORD)(m_nHostSectorCount + 2);		// Pseudo-sector number
		m_nHostSectorBuffer[m_nHostSectorCount] = nSector;	// Entity that points to the pseudo-sector
		m_nHostSectorCount++;
		m_nHostSectorCount %= XM6_HOST_PSEUDO_CLUSTER_MAX;

		return 0;
	}

	// Calculate the sector number from the cluster number
	DWORD n = nSector - (3 * cap.sectors);
	DWORD nMod = 1;
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
/// $54 - Write sector
//
//---------------------------------------------------------------------------
int CFileSys::DiskWrite(DWORD nUnit)
{

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
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
int CFileSys::Ioctrl(DWORD nUnit, DWORD nFunction, Human68k::ioctrl_t* pIoctrl)
{
	ASSERT(pIoctrl);

	// Unit check
	if (nUnit >= DriveMax)
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
				case (DWORD)-1:
					// Re-identify media
					m_cEntry.isMediaOffline(nUnit);
					return 0;

				case 0:
				case 1:
					// Dummy for Human68k compatibility
					return 0;
			}
			break;

		case (DWORD)-1:
			// Resident evaluation
			memcpy(pIoctrl->buffer, "WindrvXM", 8);
			return 0;

		case (DWORD)-2:
			// Set options
			SetOption(pIoctrl->param);
			return 0;

		case (DWORD)-3:
			// Get options
			pIoctrl->param = GetOption();
			return 0;
	}

	return FS_NOTIOCTRL;
}

//---------------------------------------------------------------------------
//
/// $56 - Flush
//
//---------------------------------------------------------------------------
int CFileSys::Flush(DWORD nUnit)
{

	// Unit check
	if (nUnit >= DriveMax)
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
int CFileSys::CheckMedia(DWORD nUnit)
{

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	if (nUnit >= m_nUnits)
		return FS_INVALIDFUNC;	// Avoid triggering a fatal error in mint when resuming with an invalid drive

	// Media change check
	BOOL bResult = m_cEntry.CheckMedia(nUnit);

	// Throw error when media is not inserted
	if (bResult == FALSE) {
		return FS_INVALIDFUNC;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
/// $58 - Lock
//
//---------------------------------------------------------------------------
int CFileSys::Lock(DWORD nUnit)
{

	// Unit check
	if (nUnit >= DriveMax)
		return FS_FATAL_INVALIDUNIT;
	ASSERT(nUnit < m_nUnits);
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
void CFileSys::SetOption(DWORD nOption)
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
	ASSERT(pArgument);

	// Initialize number of drives
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
			// Specify default base path
			if (m_nDrives < DriveMax) {
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
BOOL CFileSys::FilesVolume(DWORD nUnit, Human68k::files_t* pFiles)
{
	ASSERT(pFiles);

	// Get volume label
	TCHAR szVolume[32];
	BOOL bResult = m_cEntry.GetVolumeCache(nUnit, szVolume);
	if (bResult == FALSE) {
		// Carry out an extra media check here because it may be skipped when doing a manual eject
		if (m_cEntry.isEnable(nUnit) == FALSE)
			return FALSE;

		// Media check
		if (m_cEntry.isMediaOffline(nUnit))
			return FALSE;

		// Get volume label
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
