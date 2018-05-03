//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technorogy.
//  Copyright (C) 2016-2017 GIMONS
//	[ ホストファイルシステム ブリッジドライバ ]
//
//---------------------------------------------------------------------------

#ifndef bridge_h
#define bridge_h

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef int BOOL;

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE 1
#endif

#define FASTCALL
#define ASSERT(x)

//---------------------------------------------------------------------------
//
//	定数定義
//
//---------------------------------------------------------------------------
#define FILEPATH_MAX		_MAX_PATH
#define WINDRV_CLIPSIZE_MAX	0xC00000

//---------------------------------------------------------------------------
//
//	ステータスコード定義
//
//---------------------------------------------------------------------------
#define FS_INVALIDFUNC		0xFFFFFFFF	// 無効なファンクションコードを実行した
#define FS_FILENOTFND		0xFFFFFFFE	// 指定したファイルが見つからない
#define FS_DIRNOTFND		0xFFFFFFFD	// 指定したディレクトリが見つからない
#define FS_OVEROPENED		0xFFFFFFFC	// オープンしているファイルが多すぎる
#define FS_CANTACCESS		0xFFFFFFFB	// ディレクトリやボリュームラベルはアクセス不可
#define FS_NOTOPENED		0xFFFFFFFA	// 指定したハンドルはオープンされていない
#define FS_INVALIDMEM		0xFFFFFFF9	// メモリ管理領域が破壊された
#define FS_OUTOFMEM			0xFFFFFFF8	// 実行に必要なメモリがない
#define FS_INVALIDPTR		0xFFFFFFF7	// 無効なメモリ管理ポインタを指定した
#define FS_INVALIDENV		0xFFFFFFF6	// 不正な環境を指定した
#define FS_ILLEGALFMT		0xFFFFFFF5	// 実行ファイルのフォーマットが異常
#define FS_ILLEGALMOD		0xFFFFFFF4	// オープンのアクセスモードが異常
#define FS_INVALIDPATH		0xFFFFFFF3	// ファイル名の指定に誤りがある
#define FS_INVALIDPRM		0xFFFFFFF2	// 無効なパラメータでコールした
#define FS_INVALIDDRV		0xFFFFFFF1	// ドライブ指定に誤りがある
#define FS_DELCURDIR		0xFFFFFFF0	// カレントディレクトリは削除できない
#define FS_NOTIOCTRL		0xFFFFFFEF	// IOCTRLできないデバイス
#define FS_LASTFILE			0xFFFFFFEE	// これ以上ファイルが見つからない
#define FS_CANTWRITE		0xFFFFFFED	// 指定のファイルは書き込みできない
#define FS_DIRALREADY		0xFFFFFFEC	// 指定のディレクトリは既に登録されている
#define FS_CANTDELETE		0xFFFFFFEB	// ファイルがあるので削除できない
#define FS_CANTRENAME		0xFFFFFFEA	// ファイルがあるのでリネームできない
#define FS_DISKFULL			0xFFFFFFE9	// ディスクが一杯でファイルが作れない
#define FS_DIRFULL			0xFFFFFFE8	// ディレクトリが一杯でファイルが作れない
#define FS_CANTSEEK			0xFFFFFFE7	// 指定の位置にはシークできない
#define FS_SUPERVISOR		0xFFFFFFE6	// スーパーバイザ状態でスーパバイザ指定した
#define FS_THREADNAME		0xFFFFFFE5	// 同じスレッド名が存在する
#define FS_BUFWRITE			0xFFFFFFE4	// プロセス間通信のバッファが書込み禁止
#define FS_BACKGROUND		0xFFFFFFE3	// バックグラウンドプロセスを起動できない
#define FS_OUTOFLOCK		0xFFFFFFE0	// ロック領域が足りない
#define FS_LOCKED			0xFFFFFFDF	// ロックされていてアクセスできない
#define FS_DRIVEOPENED		0xFFFFFFDE	// 指定のドライブはハンドラがオープンされている
#define FS_LINKOVER			0xFFFFFFDD	// シンボリックリンクネストが16回を超えた
#define FS_FILEEXIST		0xFFFFFFB0	// ファイルが存在する

#define FS_FATAL_MEDIAOFFLINE	0xFFFFFFA3	// メディアが入っていない
#define FS_FATAL_WRITEPROTECT	0xFFFFFFA2	// 書き込み禁止違反
#define FS_FATAL_INVALIDCOMMAND	0xFFFFFFA1	// 不正なコマンド番号
#define FS_FATAL_INVALIDUNIT	0xFFFFFFA0	// 不正なユニット番号

#define HUMAN68K_PATH_MAX	96	// Human68kのパス最大長

//===========================================================================
//
/// Human68k 名前空間
//
//===========================================================================
/// ファイル属性ビット
enum attribute_t {
	AT_READONLY	= 0x01,		// 読み込み専用属性
	AT_HIDDEN	= 0x02,		// 隠し属性
	AT_SYSTEM	= 0x04,		// システム属性
	AT_VOLUME	= 0x08,		// ボリュームラベル属性
	AT_DIRECTORY= 0x10,		// ディレクトリ属性
	AT_ARCHIVE	= 0x20,		// アーカイブ属性
	AT_ALL		= 0xFF,		// 全ての属性ビットが1
};

/// ファイルオープンモード
enum open_t {
	OP_READ			= 0,	// 読み込み
	OP_WRITE		= 1,	// 書き込み
	OP_FULL			= 2,	// 読み書き
	OP_MASK			= 0x0F,	// 判定用マスク
	OP_SHARE_NONE	= 0x10,	// 共有禁止
	OP_SHARE_READ	= 0x20,	// 読み込み共有
	OP_SHARE_WRITE	= 0x30,	// 書き込み共有
	OP_SHARE_FULL	= 0x40,	// 読み書き共有
	OP_SHARE_MASK	= 0x70,	// 共有判定用マスク
	OP_SPECIAL		= 0x100,// 辞書アクセス
};

/// シーク種類
enum seek_t {
	SK_BEGIN	= 0,		// ファイル先頭から
	SK_CURRENT	= 1,		// 現在位置から
	SK_END		= 2,		// ファイル末尾から
};

/// メディアバイト
enum media_t {
	MEDIA_2DD_10	= 0xE0,	// 2DD/10セクタ
	MEDIA_1D_9		= 0xE5,	// 1D/9セクタ
	MEDIA_2D_9		= 0xE6,	// 2D/9セクタ
	MEDIA_1D_8		= 0xE7,	// 1D/8セクタ
	MEDIA_2D_8		= 0xE8,	// 2D/8セクタ
	MEDIA_2HT		= 0xEA,	// 2HT
	MEDIA_2HS		= 0xEB,	// 2HS
	MEDIA_2HDE		= 0xEC,	// 2DDE
	MEDIA_1DD_9		= 0xEE,	// 1DD/9セクタ
	MEDIA_1DD_8		= 0xEF,	// 1DD/8セクタ
	MEDIA_MANUAL	= 0xF1,	// リモートドライブ (手動イジェクト)
	MEDIA_REMOVABLE	= 0xF2,	// リモートドライブ (リムーバブル)
	MEDIA_REMOTE	= 0xF3,	// リモートドライブ
	MEDIA_DAT		= 0xF4,	// SCSI-DAT
	MEDIA_CDROM		= 0xF5,	// SCSI-CDROM
	MEDIA_MO		= 0xF6,	// SCSI-MO
	MEDIA_SCSI_HD	= 0xF7,	// SCSI-HD
	MEDIA_SASI_HD	= 0xF8,	// SASI-HD
	MEDIA_RAMDISK	= 0xF9,	// RAMディスク
	MEDIA_2HQ		= 0xFA,	// 2HQ
	MEDIA_2DD_8		= 0xFB,	// 2DD/8セクタ
	MEDIA_2DD_9		= 0xFC,	// 2DD/9セクタ
	MEDIA_2HC		= 0xFD,	// 2HC
	MEDIA_2HD		= 0xFE,	// 2HD
};

/// namests構造体
typedef struct {
	BYTE wildcard;			// ワイルドカード文字数
	BYTE drive;				// ドライブ番号
	BYTE path[65];			// パス(サブディレクトリ+/)
	BYTE name[8];			// ファイル名 (PADDING 0x20)
	BYTE ext[3];			// 拡張子 (PADDING 0x20)
	BYTE add[10];			// ファイル名追加 (PADDING 0x00)
} namests_t;

/// files構造体
typedef struct {
	BYTE fatr;				// + 0 検索する属性			読込専用
	BYTE pad1[3];			// padding
//		BYTE drive;			// + 1 ドライブ番号			読込専用
	DWORD sector;			// + 2 ディレクトリのセクタ	DOS _FILES先頭アドレスで代用
//		WORD cluster;		// + 6 ディレクトリのクラスタ	詳細不明 (未使用)
	WORD offset;			// + 8 ディレクトリエントリ	書込専用
//		BYTE name[8];		// +10 作業用ファイル名		読込専用 (未使用)
//		BYTE ext[3];		// +18 作業用拡張子			読込専用 (未使用)
	BYTE attr;				// +21 ファイル属性			書込専用
	BYTE pad2;				// padding
	WORD time;				// +22 最終変更時刻			書込専用
	WORD date;				// +24 最終変更月日			書込専用
	DWORD size;				// +26 ファイルサイズ			書込専用
	BYTE full[23];			// +30 フルファイル名			書込専用
	BYTE pad3;				// padding
} files_t;

/// FCB構造体
typedef struct {
//		BYTE pad00[6];		// + 0～+ 5	(未使用)
	DWORD fileptr;			// + 6～+ 9	ファイルポインタ
//		BYTE pad01[4];		// +10～+13	(未使用)
	WORD mode;				// +14～+15	オープンモード
//		BYTE pad02[16];		// +16～+31	(未使用)
//		DWORD zero;			// +32～+35	オープンのとき0が書き込まれている (未使用)
//		BYTE name[8];		// +36～+43	ファイル名 (PADDING 0x20) (未使用)
//		BYTE ext[3];		// +44～+46	拡張子 (PADDING 0x20) (未使用)
	BYTE attr;				// +47		ファイル属性
	BYTE pad;				// padding
//		BYTE add[10];		// +48～+57	ファイル名追加 (PADDING 0x00) (未使用)
	WORD time;				// +58～+59	最終変更時刻
	WORD date;				// +60～+61	最終変更月日
//		WORD cluster;		// +62～+63	クラスタ番号 (未使用)
	DWORD size;				// +64～+67	ファイルサイズ
//		BYTE pad03[28];		// +68～+95	FATキャッシュ (未使用)
} fcb_t;

/// capacity構造体
typedef struct {
	WORD freearea;			// + 0	使用可能なクラスタ数
	WORD clusters;			// + 2	総クラスタ数
	WORD sectors;			// + 4	クラスタあたりのセクタ数
	WORD bytes;				// + 6	セクタ当たりのバイト数
} capacity_t;

/// ctrldrive構造体
typedef struct {
	BYTE status;			// +13	状態
	BYTE pad[3];			// padding
} ctrldrive_t;

/// DPB構造体
typedef struct {
	WORD sector_size;		// + 0	1セクタ当りのバイト数
	BYTE cluster_size;		// + 2	1クラスタ当りのセクタ数-1
	BYTE shift;				// + 3	クラスタ→セクタのシフト数
	WORD fat_sector;		// + 4	FATの先頭セクタ番号
	BYTE fat_max;			// + 6	FAT領域の個数
	BYTE fat_size;			// + 7	FATの占めるセクタ数(複写分を除く)
	WORD file_max;			// + 8	ルートディレクトリに入るファイルの個数
	WORD data_sector;		// +10	データ領域の先頭セクタ番号
	WORD cluster_max;		// +12	総クラスタ数+1
	WORD root_sector;		// +14	ルートディレクトリの先頭セクタ番号
//		DWORD driverentry;	// +16	デバイスドライバへのポインタ (未使用)
	BYTE media;				// +20	メディア識別子
//		BYTE flag;			// +21	DPB使用フラグ (未使用)
	BYTE pad;				// padding
} dpb_t;

/// ディレクトリエントリ構造体
typedef struct {
	BYTE name[8];			// + 0	ファイル名 (PADDING 0x20)
	BYTE ext[3];			// + 8	拡張子 (PADDING 0x20)
	BYTE attr;				// +11	ファイル属性
	BYTE add[10];			// +12	ファイル名追加 (PADDING 0x00)
	WORD time;				// +22	最終変更時刻
	WORD date;				// +24	最終変更月日
	WORD cluster;			// +26	クラスタ番号
	DWORD size;				// +28	ファイルサイズ
} dirent_t;

/// IOCTRLパラメータ共用体
typedef union {
	BYTE buffer[8];			// バイト単位でのアクセス
	DWORD param;			// パラメータ(先頭4バイト)
	WORD media;				// メディアバイト(先頭2バイト)
} ioctrl_t;

/// コマンドライン引数構造体
typedef struct {
	BYTE buf[256];			// コマンドライン引数
} argument_t;

#endif	// bridge_h
