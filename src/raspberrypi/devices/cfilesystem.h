//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ Host File System for the X68000 ]
//
//  Note: This functionality is specific to the X68000 operating system.
//        It is highly unlikely that this will work for other platforms.
//---------------------------------------------------------------------------

#ifndef cfilesystem_h
#define cfilesystem_h

//---------------------------------------------------------------------------
//
//	ステータスコード定義
//
//---------------------------------------------------------------------------
#define FS_INVALIDFUNC		0xFFFFFFFF	///< 無効なファンクションコードを実行した
#define FS_FILENOTFND		0xFFFFFFFE	///< 指定したファイルが見つからない
#define FS_DIRNOTFND		0xFFFFFFFD	///< 指定したディレクトリが見つからない
#define FS_OVEROPENED		0xFFFFFFFC	///< オープンしているファイルが多すぎる
#define FS_CANTACCESS		0xFFFFFFFB	///< ディレクトリやボリュームラベルはアクセス不可
#define FS_NOTOPENED		0xFFFFFFFA	///< 指定したハンドルはオープンされていない
#define FS_INVALIDMEM		0xFFFFFFF9	///< メモリ管理領域が破壊された
#define FS_OUTOFMEM		0xFFFFFFF8	///< 実行に必要なメモリがない
#define FS_INVALIDPTR		0xFFFFFFF7	///< 無効なメモリ管理ポインタを指定した
#define FS_INVALIDENV		0xFFFFFFF6	///< 不正な環境を指定した
#define FS_ILLEGALFMT		0xFFFFFFF5	///< 実行ファイルのフォーマットが異常
#define FS_ILLEGALMOD		0xFFFFFFF4	///< オープンのアクセスモードが異常
#define FS_INVALIDPATH		0xFFFFFFF3	///< ファイル名の指定に誤りがある
#define FS_INVALIDPRM		0xFFFFFFF2	///< 無効なパラメータでコールした
#define FS_INVALIDDRV		0xFFFFFFF1	///< ドライブ指定に誤りがある
#define FS_DELCURDIR		0xFFFFFFF0	///< カレントディレクトリは削除できない
#define FS_NOTIOCTRL		0xFFFFFFEF	///< IOCTRLできないデバイス
#define FS_LASTFILE		0xFFFFFFEE	///< これ以上ファイルが見つからない
#define FS_CANTWRITE		0xFFFFFFED	///< 指定のファイルは書き込みできない
#define FS_DIRALREADY		0xFFFFFFEC	///< 指定のディレクトリは既に登録されている
#define FS_CANTDELETE		0xFFFFFFEB	///< ファイルがあるので削除できない
#define FS_CANTRENAME		0xFFFFFFEA	///< ファイルがあるのでリネームできない
#define FS_DISKFULL		0xFFFFFFE9	///< ディスクが一杯でファイルが作れない
#define FS_DIRFULL		0xFFFFFFE8	///< ディレクトリが一杯でファイルが作れない
#define FS_CANTSEEK		0xFFFFFFE7	///< 指定の位置にはシークできない
#define FS_SUPERVISOR		0xFFFFFFE6	///< スーパーバイザ状態でスーパバイザ指定した
#define FS_THREADNAME		0xFFFFFFE5	///< 同じスレッド名が存在する
#define FS_BUFWRITE		0xFFFFFFE4	///< プロセス間通信のバッファが書込み禁止
#define FS_BACKGROUND		0xFFFFFFE3	///< バックグラウンドプロセスを起動できない
#define FS_OUTOFLOCK		0xFFFFFFE0	///< ロック領域が足りない
#define FS_LOCKED		0xFFFFFFDF	///< ロックされていてアクセスできない
#define FS_DRIVEOPENED		0xFFFFFFDE	///< 指定のドライブはハンドラがオープンされている
#define FS_LINKOVER		0xFFFFFFDD	///< シンボリックリンクネストが16回を超えた
#define FS_FILEEXIST		0xFFFFFFB0	///< ファイルが存在する

#define FS_FATAL_MEDIAOFFLINE	0xFFFFFFA3	///< メディアが入っていない
#define FS_FATAL_WRITEPROTECT	0xFFFFFFA2	///< 書き込み禁止違反
#define FS_FATAL_INVALIDCOMMAND	0xFFFFFFA1	///< 不正なコマンド番号
#define FS_FATAL_INVALIDUNIT	0xFFFFFFA0	///< 不正なユニット番号

#define HUMAN68K_PATH_MAX	96		///< Human68kのパス最大長

//===========================================================================
//
/// Human68k 名前空間
//
//===========================================================================
namespace Human68k {
	/// ファイル属性ビット
	enum attribute_t {
		AT_READONLY = 0x01,		///< 読み込み専用属性
		AT_HIDDEN = 0x02,		///< 隠し属性
		AT_SYSTEM = 0x04,		///< システム属性
		AT_VOLUME = 0x08,		///< ボリュームラベル属性
		AT_DIRECTORY = 0x10,		///< ディレクトリ属性
		AT_ARCHIVE = 0x20,		///< アーカイブ属性
		AT_ALL = 0xFF,			///< 全ての属性ビットが1
	};

	/// ファイルオープンモード
	enum open_t {
		OP_READ = 0,			///< 読み込み
		OP_WRITE = 1,			///< 書き込み
		OP_FULL = 2,			///< 読み書き
		OP_MASK = 0x0F,			///< 判定用マスク
		OP_SHARE_NONE = 0x10,		///< 共有禁止
		OP_SHARE_READ = 0x20,		///< 読み込み共有
		OP_SHARE_WRITE = 0x30,		///< 書き込み共有
		OP_SHARE_FULL = 0x40,		///< 読み書き共有
		OP_SHARE_MASK = 0x70,		///< 共有判定用マスク
		OP_SPECIAL = 0x100,		///< 辞書アクセス
	};

	/// シーク種類
	enum seek_t {
		SK_BEGIN = 0,			///< ファイル先頭から
		SK_CURRENT = 1,			///< 現在位置から
		SK_END = 2,			///< ファイル末尾から
	};

	/// メディアバイト
	enum media_t {
		MEDIA_2DD_10 = 0xE0,		///< 2DD/10セクタ
		MEDIA_1D_9 = 0xE5,		///< 1D/9セクタ
		MEDIA_2D_9 = 0xE6,		///< 2D/9セクタ
		MEDIA_1D_8 = 0xE7,		///< 1D/8セクタ
		MEDIA_2D_8 = 0xE8,		///< 2D/8セクタ
		MEDIA_2HT = 0xEA,		///< 2HT
		MEDIA_2HS = 0xEB,		///< 2HS
		MEDIA_2HDE = 0xEC,		///< 2DDE
		MEDIA_1DD_9 = 0xEE,		///< 1DD/9セクタ
		MEDIA_1DD_8 = 0xEF,		///< 1DD/8セクタ
		MEDIA_MANUAL = 0xF1,		///< リモートドライブ (手動イジェクト)
		MEDIA_REMOVABLE = 0xF2,		///< リモートドライブ (リムーバブル)
		MEDIA_REMOTE = 0xF3,		///< リモートドライブ
		MEDIA_DAT = 0xF4,		///< SCSI-DAT
		MEDIA_CDROM = 0xF5,		///< SCSI-CDROM
		MEDIA_MO = 0xF6,		///< SCSI-MO
		MEDIA_SCSI_HD = 0xF7,		///< SCSI-HD
		MEDIA_SASI_HD = 0xF8,		///< SASI-HD
		MEDIA_RAMDISK = 0xF9,		///< RAMディスク
		MEDIA_2HQ = 0xFA,		///< 2HQ
		MEDIA_2DD_8 = 0xFB,		///< 2DD/8セクタ
		MEDIA_2DD_9 = 0xFC,		///< 2DD/9セクタ
		MEDIA_2HC = 0xFD,		///< 2HC
		MEDIA_2HD = 0xFE,		///< 2HD
	};

	/// namests構造体
	struct namests_t {
		BYTE wildcard;			///< ワイルドカード文字数
		BYTE drive;			///< ドライブ番号
		BYTE path[65];			///< パス(サブディレクトリ+/)
		BYTE name[8];			///< ファイル名 (PADDING 0x20)
		BYTE ext[3];			///< 拡張子 (PADDING 0x20)
		BYTE add[10];			///< ファイル名追加 (PADDING 0x00)

		// 文字列取得
		void GetCopyPath(BYTE* szPath) const;
		///< パス名取得
		void GetCopyFilename(BYTE* szFilename) const;
		///< ファイル名取得
	};

	/// files構造体
	struct files_t {
		BYTE fatr;			///< + 0 検索する属性			読込専用
		//		BYTE drive;	///< + 1 ドライブ番号			読込専用
		DWORD sector;			///< + 2 ディレクトリのセクタ		DOS _FILES先頭アドレスで代用
		//		WORD cluster;	///< + 6 ディレクトリのクラスタ		詳細不明 (未使用)
		WORD offset;			///< + 8 ディレクトリエントリ		書込専用
		//		BYTE name[8];	///< +10 作業用ファイル名		読込専用 (未使用)
		//		BYTE ext[3];	///< +18 作業用拡張子			読込専用 (未使用)
		BYTE attr;			///< +21 ファイル属性			書込専用
		WORD time;			///< +22 最終変更時刻			書込専用
		WORD date;			///< +24 最終変更月日			書込専用
		DWORD size;			///< +26 ファイルサイズ			書込専用
		BYTE full[23];			///< +30 フルファイル名			書込専用
	};

	/// FCB構造体
	struct fcb_t {
		//		BYTE pad00[6];	///< + 0～+ 5	(未使用)
		DWORD fileptr;			///< + 6～+ 9	ファイルポインタ
		//		BYTE pad01[4];	///< +10～+13	(未使用)
		WORD mode;			///< +14～+15	オープンモード
		//		BYTE pad02[16];	///< +16～+31	(未使用)
		//		DWORD zero;	///< +32～+35	オープンのとき0が書き込まれている (未使用)
		//		BYTE name[8];	///< +36～+43	ファイル名 (PADDING 0x20) (未使用)
		//		BYTE ext[3];	///< +44～+46	拡張子 (PADDING 0x20) (未使用)
		BYTE attr;			///< +47	ファイル属性
		//		BYTE add[10];	///< +48～+57	ファイル名追加 (PADDING 0x00) (未使用)
		WORD time;			///< +58～+59	最終変更時刻
		WORD date;			///< +60～+61	最終変更月日
		//		WORD cluster;	///< +62～+63	クラスタ番号 (未使用)
		DWORD size;			///< +64～+67	ファイルサイズ
		//		BYTE pad03[28];	///< +68～+95	FATキャッシュ (未使用)
	};

	/// capacity構造体
	struct capacity_t {
		WORD freearea;			///< + 0	使用可能なクラスタ数
		WORD clusters;			///< + 2	総クラスタ数
		WORD sectors;			///< + 4	クラスタあたりのセクタ数
		WORD bytes;			///< + 6	セクタ当たりのバイト数
	};

	/// ctrldrive構造体
	struct ctrldrive_t {
		BYTE status;			///< +13	状態
		BYTE pad[3];			///< Padding
	};

	/// DPB構造体
	struct dpb_t {
		WORD sector_size;		///< + 0	1セクタ当りのバイト数
		BYTE cluster_size;		///< + 2	1クラスタ当りのセクタ数-1
		BYTE shift;			///< + 3	クラスタ→セクタのシフト数
		WORD fat_sector;		///< + 4	FATの先頭セクタ番号
		BYTE fat_max;			///< + 6	FAT領域の個数
		BYTE fat_size;			///< + 7	FATの占めるセクタ数(複写分を除く)
		WORD file_max;			///< + 8	ルートディレクトリに入るファイルの個数
		WORD data_sector;		///< +10	データ領域の先頭セクタ番号
		WORD cluster_max;		///< +12	総クラスタ数+1
		WORD root_sector;		///< +14	ルートディレクトリの先頭セクタ番号
		//	DWORD driverentry;	///< +16	デバイスドライバへのポインタ (未使用)
		BYTE media;			///< +20	メディア識別子
		//	BYTE flag;		///< +21	DPB使用フラグ (未使用)
	};

	/// ディレクトリエントリ構造体
	struct dirent_t {
		BYTE name[8];			///< + 0	ファイル名 (PADDING 0x20)
		BYTE ext[3];			///< + 8	拡張子 (PADDING 0x20)
		BYTE attr;			///< +11	ファイル属性
		BYTE add[10];			///< +12	ファイル名追加 (PADDING 0x00)
		WORD time;			///< +22	最終変更時刻
		WORD date;			///< +24	最終変更月日
		WORD cluster;			///< +26	クラスタ番号
		DWORD size;			///< +28	ファイルサイズ
	};

	/// IOCTRLパラメータ共用体
	union ioctrl_t {
		BYTE buffer[8];			///< バイト単位でのアクセス
		DWORD param;			///< パラメータ(先頭4バイト)
		WORD media;			///< メディアバイト(先頭2バイト)
	};

	/// コマンドライン引数構造体
	/**
	先頭にドライバ自身のパスが含まれるためHUMAN68K_PATH_MAX以上のサイズにする。
	*/
	struct argument_t {
		BYTE buf[256];			///< コマンドライン引数
	};
}

/// FILES用バッファ個数
/**
通常は数個で十分だが、Human68kの複数のプロセスがマルチタスクで同時に
深い階層に渡って作業する時などはこの値を増やす必要がある。

デフォルトは20個。
*/
#define XM6_HOST_FILES_MAX	20

/// FCB用バッファ個数
/**
同時にオープンできるファイル数はこれで決まる。

デフォルトは100ファイル。
*/
#define XM6_HOST_FCB_MAX	100

/// 仮想セクタ/クラスタ 最大個数
/**
ファイル実体の先頭セクタへのアクセスに対応するための仮想セクタの個数。
lzdsysによるアクセスを行なうスレッドの数より多めに確保する。

デフォルトは10セクタ。
*/
#define XM6_HOST_PSEUDO_CLUSTER_MAX	10

/// ディレクトリエントリ キャッシュ個数
/**
Human68kは、サブディレクトリ内で処理を行なう際にディレクトリエントリ
のチェックを大量に発行する。この応答を高速化するための簡易キャッシュ
の個数を指定する。キャッシュは各ドライブ毎に確保される。
多いほど高速になるが、増やしすぎるとホストOS側に負担がかかるので注意。

デフォルトは16個。
*/
#define XM6_HOST_DIRENTRY_CACHE_MAX	16

/// 1ディレクトリに収納できるエントリの最大数
/**
ディレクトリ内にファイルが大量に存在すると、当時のアプリケーションが
想定していない大量のデータを返してしまうことになる。アプリによっては
一部しか認識されなかったり、速度が大幅に低下したり、メモリ不足で停止
するなどの危険性が存在する。このため上限を設定することで対処する。
例えばとあるファイラの場合、2560ファイルが上限となっている。この数を
一つの目安とするのが良い。

デフォルトは約6万エントリ。(FATのルートディレクトリでの上限値)
*/
#define XM6_HOST_DIRENTRY_FILE_MAX	65535

/// ファイル名の重複除外パターンの最大数
/**
Human68k側のファイル名は、ホスト側のファイルシステムの名称をもとに自
動生成されるが、Human68k側のファイル名よりもホスト側のファイル名の名
称のほうが長いため、同名のファイル名が生成されてしまう可能性がある。
その時、Human68k側からファイル名を区別できるようにするため、WindrvXM
独自の命名規則に従って別名を生成して解決している。
理論上は約6千万(36の5乗)通りの別名を生成できる方式を取っているが、実
際には数百パターン以上の重複判定が発生すると処理に時間がかかってしま
うため、重複の上限を設定することで速度を維持する。常識的な運用であれ
ば、代替名は数パターンもあれば十分運用できるはずであり、この値を可能
な限り小さい値にすることでパフォーマンスの改善が期待できる。
この個数を超えるファイル名が重複してしまった場合は、同名のエントリが
複数生成される。この場合、ファイル一覧では見えるがファイル名で指定す
ると最初のエントリのみ扱える状態となる。

デフォルトは36パターン。
*/
#define XM6_HOST_FILENAME_PATTERN_MAX	36

/// ファイル名重複防止マーク
/**
ホスト側のファイル名とHuman68k側ファイル名の名称の区別をつけるときに
使う文字。コマンドシェル等のエスケープ文字と重ならないものを選ぶと吉。

デフォルトは「@」。
*/
#define XM6_HOST_FILENAME_MARK	'@'

/// WINDRV動作フラグ
/**
通常は0にする。ファイル削除にOSのごみ箱機能を利用する場合は1にする。
それ以外の値は将来のための予約とする。
内部動作フラグとメディアバイト偽装などを見越した将来の拡張用。
*/
enum {
	WINDRV_OPT_REMOVE =		0x00000001,	///< Bit 0: ファイル削除処理			0:直接 1:ごみ箱
	WINDRV_OPT_ALPHABET =		0x00000020,	///< Bit 5: ファイル名比較 Alphabet区別		0:なし 1:あり	0:-C 1:+C
	WINDRV_OPT_COMPARE_LENGTH =	0x00000040,	///< Bit 6: ファイル名比較 文字数(未実装)	0:18+3 1:8+3	0:+T 1:-T
	WINDRV_OPT_CONVERT_LENGTH =	0x00000080,	///< Bit 7: ファイル名変換 文字数		0:18+3 1:8+3	0:-A 1:+A
	WINDRV_OPT_CONVERT_SPACE =	0x00000100,	///< Bit 8: ファイル名変換 スペース		0:なし 1:'_'
	WINDRV_OPT_CONVERT_BADCHAR =	0x00000200,	///< Bit 9: ファイル名変換 無効な文字		0:なし 1:'_'
	WINDRV_OPT_CONVERT_HYPHENS =	0x00000400,	///< Bit10: ファイル名変換 中間のハイフン	0:なし 1:'_'
	WINDRV_OPT_CONVERT_HYPHEN =	0x00000800,	///< Bit11: ファイル名変換 先頭のハイフン	0:なし 1:'_'
	WINDRV_OPT_CONVERT_PERIODS =	0x00001000,	///< Bit12: ファイル名変換 中間のピリオド	0:なし 1:'_'
	WINDRV_OPT_CONVERT_PERIOD =	0x00002000,	///< Bit13: ファイル名変換 先頭のピリオド	0:なし 1:'_'
	WINDRV_OPT_REDUCED_SPACE =	0x00010000,	///< Bit16: ファイル名短縮 スペース		0:なし 1:短縮
	WINDRV_OPT_REDUCED_BADCHAR =	0x00020000,	///< Bit17: ファイル名短縮 無効な文字		0:なし 1:短縮
	WINDRV_OPT_REDUCED_HYPHENS =	0x00040000,	///< Bit18: ファイル名短縮 中間のハイフン	0:なし 1:短縮
	WINDRV_OPT_REDUCED_HYPHEN =	0x00080000,	///< Bit19: ファイル名短縮 先頭のハイフン	0:なし 1:短縮
	WINDRV_OPT_REDUCED_PERIODS =	0x00100000,	///< Bit20: ファイル名短縮 中間のピリオド	0:なし 1:短縮
	WINDRV_OPT_REDUCED_PERIOD =	0x00200000,	///< Bit21: ファイル名短縮 先頭のピリオド	0:なし 1:短縮
	// Bit24～30 ファイル重複防止マーク			0:自動 1～127:文字
};

/// ファイルシステム動作フラグ
/**
通常は0にする。リードオンリーでマウントしたいドライブの場合は1にする。
それ以外の値は将来のための予約とする。
判定が困難なデバイス(自作USBストレージとか)のための保険用。
*/
enum {
	FSFLAG_WRITE_PROTECT =		0x00000001,	///< Bit0: 強制書き込み禁止
	FSFLAG_REMOVABLE =		0x00000002,	///< Bit1: 強制リムーバブルメディア
	FSFLAG_MANUAL =			0x00000004,	///< Bit2: 強制手動イジェクト
};

//===========================================================================
//
/// まるっとリングリスト
///
/// 先頭(root.next)が最も新しいオブジェクト。
/// 末尾(root.prev)が最も古い/未使用オブジェクト。
/// コード効率追求のため、delete時は必ずポインタをアップキャストすること。
//
//===========================================================================
class CRing {
public:
	// 基本ファンクション
	CRing() { Init(); }							///< デフォルトコンストラクタ
	~CRing() { Remove(); }							///< デストラクタ final
	void  Init() { next = prev = this; }			///< 初期化

	CRing*  Next() const { return next; }			///< 次の要素を取得
	CRing*  Prev() const { return prev; }			///< 前の要素を取得

	void  Insert(CRing* pRoot)
	{
			// 該当オブジェクトを切り離し
		ASSERT(next);
		ASSERT(prev);
		next->prev = prev;
		prev->next = next;
		// リング先頭へ挿入
		ASSERT(pRoot);
		ASSERT(pRoot->next);
		next = pRoot->next;
		prev = pRoot;
		pRoot->next->prev = this;
		pRoot->next = this;
	}
										///< オブジェクト切り離し & リング先頭へ挿入

	void  InsertTail(CRing* pRoot)
	{
			// 該当オブジェクトを切り離し
		ASSERT(next);
		ASSERT(prev);
		next->prev = prev;
		prev->next = next;
		// リング末尾へ挿入
		ASSERT(pRoot);
		ASSERT(pRoot->prev);
		next = pRoot;
		prev = pRoot->prev;
		pRoot->prev->next = this;
		pRoot->prev = this;
	}
										///< オブジェクト切り離し & リング末尾へ挿入

	void  InsertRing(CRing* pRoot)
	{
			if (next == prev) return;

		// リング先頭へ挿入
		ASSERT(pRoot);
		ASSERT(pRoot->next);
		pRoot->next->prev = prev;
		prev->next = pRoot->next;
		pRoot->next = next;
		next->prev = pRoot;

		// 自分自身を空にする
		next = prev = this;
	}
										///< 自分以外のオブジェクト切り離し & リング先頭へ挿入

	void  Remove()
	{
			// 該当オブジェクトを切り離し
		ASSERT(next);
		ASSERT(prev);
		next->prev = prev;
		prev->next = next;
		// 安全のため自分自身を指しておく (何度切り離しても問題ない)
		next = prev = this;
	}
										///< オブジェクト切り離し

private:
	CRing* next;						///< 次の要素
	CRing* prev;						///< 前の要素
};

//===========================================================================
//
/// ディレクトリエントリ ファイル名
//
//===========================================================================
class CHostFilename {
public:
	// 基本ファンクション
	CHostFilename();							///< デフォルトコンストラクタ
	static size_t  Offset() { return offsetof(CHostFilename, m_szHost); }	///< オフセット位置取得

	void  SetHost(const TCHAR* szHost);					///< ホスト側の名称を設定
	const TCHAR*  GetHost() const { return m_szHost; }	///< ホスト側の名称を取得
	void  ConvertHuman(int nCount = -1);					///< Human68k側の名称を変換
	void  CopyHuman(const BYTE* szHuman);					///< Human68k側の名称を複製
	BOOL  isReduce() const;							///< Human68k側の名称が加工されたか調査
	BOOL  isCorrect() const { return m_bCorrect; }		///< Human68k側のファイル名規則に合致しているか調査
	const BYTE*  GetHuman() const { return m_szHuman; }	///< Human68kファイル名を取得
	const BYTE*  GetHumanLast() const 
	{ return m_pszHumanLast; }				///< Human68kファイル名を取得
	const BYTE*  GetHumanExt() const { return m_pszHumanExt; }///< Human68kファイル名を取得
	void  SetEntryName();							///< Human68kディレクトリエントリを設定
	void  SetEntryAttribute(BYTE nHumanAttribute)
	{ m_dirHuman.attr = nHumanAttribute; }			///< Human68kディレクトリエントリを設定
	void  SetEntrySize(DWORD nHumanSize)
	{ m_dirHuman.size = nHumanSize; }				///< Human68kディレクトリエントリを設定
	void  SetEntryDate(WORD nHumanDate)
	{ m_dirHuman.date = nHumanDate; }				///< Human68kディレクトリエントリを設定
	void  SetEntryTime(WORD nHumanTime)
	{ m_dirHuman.time = nHumanTime; }				///< Human68kディレクトリエントリを設定
	void  SetEntryCluster(WORD nHumanCluster)
	{ m_dirHuman.cluster = nHumanCluster; }			///< Human68kディレクトリエントリを設定
	const Human68k::dirent_t*  GetEntry() const 
	{ return &m_dirHuman; }					///< Human68kディレクトリエントリを取得
	BOOL  CheckAttribute(DWORD nHumanAttribute) const;			///< Human68kディレクトリエントリの属性判定
	BOOL  isSameEntry(const Human68k::dirent_t* pdirHuman) const
	{ ASSERT(pdirHuman); return memcmp(&m_dirHuman, pdirHuman, sizeof(m_dirHuman)) == 0; }
										///< Human68kディレクトリエントリの一致判定

	// パス名操作
	static const BYTE*  SeparateExt(const BYTE* szHuman);			///< Human68kファイル名から拡張子を分離

private:
	static BYTE*  CopyName(BYTE* pWrite, const BYTE* pFirst, const BYTE* pLast);
										///< Human68k側のファイル名要素をコピー

	const BYTE* m_pszHumanLast;		///< 該当エントリのHuman68k内部名の終端位置
	const BYTE* m_pszHumanExt;		///< 該当エントリのHuman68k内部名の拡張子位置
	BOOL m_bCorrect;			///< 該当エントリのHuman68k内部名が正しければ真
	BYTE m_szHuman[24];			///< 該当エントリのHuman68k内部名
	Human68k::dirent_t m_dirHuman;		///< 該当エントリのHuman68k全情報
	TCHAR m_szHost[FILEPATH_MAX];		///< 該当エントリのホスト側の名称 (可変長)
};

//===========================================================================
//
/// ディレクトリエントリ パス名
///
/// Human68k側のパス名は、必ず先頭が/で始まり、末尾が/で終わる。
/// ユニット番号は持たない。
/// 高速化のため、ホスト側の名称にはベースパス部分も含む。
//
//===========================================================================
/** @note
ほとんどのHuman68kのアプリは、ファイルの更新等によってディレクトリエ
ントリに変更が生じた際、親ディレクトリのタイムスタンプは変化しないも
のと想定して作られている。
ところがホスト側のファイルシステムでは親ディレクトリのタイムスタンプ
も変化してしまうものが主流となってしまっている。

このため、ディレクトリのコピー等において、アプリ側は正確にタイムスタ
ンプ情報などを設定しているにもかかわらず、実行結果では時刻情報が上書
きされてしまうという惨劇が起きてしまう。

そこでディレクトリキャッシュ内にFATタイムスタンプのエミュレーション
機能を実装した。ホスト側のファイルシステムの更新時にタイムスタンプ情
報を復元することでHuman68k側の期待する結果と一致させる。
*/
class CHostPath: public CRing {
	/// メモリ管理用
	struct ring_t {
		CRing r;			///< 円環
		CHostFilename f;		///< 実体
	};

public:
	/// 検索用バッファ
	struct find_t {
		DWORD count;			///< 検索実行回数 + 1 (0のときは以下の値は無効)
		DWORD id;			///< 次回検索を続行するパスのエントリ識別ID
		const ring_t* pos;		///< 次回検索を続行する位置 (識別ID一致時)
		Human68k::dirent_t entry;	///< 次回検索を続行するエントリ内容

		void  Clear() { count = 0; }	///< 初期化
	};

	// 基本ファンクション
	CHostPath();								///< デフォルトコンストラクタ
	~CHostPath();								///< デストラクタ final
	void  Clean();								///< 再利用のための初期化

	void  SetHuman(const BYTE* szHuman);					///< Human68k側の名称を直接指定する
	void  SetHost(const TCHAR* szHost);					///< ホスト側の名称を直接指定する
	BOOL  isSameHuman(const BYTE* szHuman) const;				///< Human68k側の名称を比較する
	BOOL  isSameChild(const BYTE* szHuman) const;				///< Human68k側の名称を比較する
	const TCHAR*  GetHost() const { return m_szHost; }	///< ホスト側の名称の獲得
	const CHostFilename*  FindFilename(const BYTE* szHuman, DWORD nHumanAttribute = Human68k::AT_ALL) const;
										///< ファイル名を検索
	const CHostFilename*  FindFilenameWildcard(const BYTE* szHuman, DWORD nHumanAttribute, find_t* pFind) const;
										///< ファイル名を検索 (ワイルドカード対応)
	BOOL  isRefresh();							///< ファイル変更が行なわれたか確認
	void  Refresh();							///< ファイル再構成
	void  Backup();								/// ホスト側のタイムスタンプを保存
	void  Restore() const;							/// ホスト側のタイムスタンプを復元
	void  Release();							///< 更新

	// CHostEntryが利用する外部API
	static void  InitId() { g_nId = 0; }					///< 識別ID生成用カウンタ初期化

private:
	static ring_t*  Alloc(size_t nLength);					///< ファイル名領域確保
	static void  Free(ring_t* pRing);					///< ファイル名領域解放
	static int  Compare(const BYTE* pFirst, const BYTE* pLast, const BYTE* pBufFirst, const BYTE* pBufLast);
										///< 文字列比較 (ワイルドカード対応)

	CRing m_cRing;								///< CHostFilename連結用
	time_t m_tBackup;							///< 時刻復元用
	BOOL m_bRefresh;							///< 更新フラグ
	DWORD m_nId;								///< 識別ID (値が変化した場合は更新を意味する)
	BYTE m_szHuman[HUMAN68K_PATH_MAX];					///< 該当エントリのHuman68k内部名
	TCHAR m_szHost[FILEPATH_MAX];						///< 該当エントリのホスト側の名称

	static DWORD g_nId;							///< 識別ID生成用カウンタ
};

//===========================================================================
//
/// ファイル検索処理
///
/// Human68k側のファイル名を内部Unicodeで処理するのは正直キツい。と
/// いうわけで、全てBYTEに変換して処理する。変換処理はディレクトリエ
/// ントリキャッシュが一手に担い、WINDRV側はすべてシフトJISのみで扱
/// えるようにする。
/// また、Human68k側名称は、完全にベースパス指定から独立させる。
///
/// ファイルを扱う直前に、ディレクトリエントリのキャッシュを生成する。
/// ディレクトリエントリの生成処理は高コストのため、一度生成したエントリは
/// 可能な限り維持して使い回す。
///
/// ファイル検索は3方式。すべてCHostFiles::Find()で処理する。
/// 1. パス名のみ検索 属性はディレクトリのみ _CHKDIR _CREATE
/// 2. パス名+ファイル名+属性の検索 _OPEN
/// 3. パス名+ワイルドカード+属性の検索 _FILES _NFILES
/// 検索結果は、ディレクトリエントリ情報として保持しておく。
//
//===========================================================================
class CHostFiles {
public:
	// 基本ファンクション
	CHostFiles() { SetKey(0); Init(); }						///< デフォルトコンストラクタ
	void  Init();									///< 初期化

	void  SetKey(DWORD nKey) { m_nKey = nKey; }			///< 検索キー設定
	BOOL  isSameKey(DWORD nKey) const { return m_nKey == nKey; }	///< 検索キー比較
	void  SetPath(const Human68k::namests_t* pNamests);				///< パス名・ファイル名を内部で生成
	BOOL  isRootPath() const { return m_szHumanPath[1] == '\0'; }			///< ルートディレクトリ判定
	void  SetPathWildcard() { m_nHumanWildcard = 1; }				///< ワイルドカードによるファイル検索を有効化
	void  SetPathOnly() { m_nHumanWildcard = 0xFF; }				///< パス名のみを有効化
	BOOL  isPathOnly() const { return m_nHumanWildcard == 0xFF; }			///< パス名のみ設定か判定
	void  SetAttribute(DWORD nHumanAttribute) { m_nHumanAttribute = nHumanAttribute; }
											///< 検索属性を設定
	BOOL  Find(DWORD nUnit, class CHostEntry* pEntry);				///< Human68k側でファイルを検索しホスト側の情報を生成
	const CHostFilename*  Find(CHostPath* pPath);					///< ファイル名検索
	void  SetEntry(const CHostFilename* pFilename);					///< Human68k側の検索結果保存
	void  SetResult(const TCHAR* szPath);						///< ホスト側の名称を設定
	void  AddResult(const TCHAR* szPath);						///< ホスト側の名称にファイル名を追加
	void  AddFilename();								///< ホスト側の名称にHuman68kの新規ファイル名を追加

	const TCHAR*  GetPath() const { return m_szHostResult; }		///< ホスト側の名称を取得

	const Human68k::dirent_t*  GetEntry() const { return &m_dirHuman; }///< Human68kディレクトリエントリを取得

	DWORD  GetAttribute() const { return m_dirHuman.attr; }		///< Human68k属性を取得
	WORD  GetDate() const { return m_dirHuman.date; }			///< Human68k日付を取得
	WORD  GetTime() const { return m_dirHuman.time; }			///< Human68k時刻を取得
	DWORD  GetSize() const { return m_dirHuman.size; }		///< Human68kファイルサイズを取得
	const BYTE*  GetHumanFilename() const { return m_szHumanFilename; }///< Human68kファイル名を取得
	const BYTE*  GetHumanResult() const { return m_szHumanResult; }	///< Human68kファイル名検索結果を取得
	const BYTE*  GetHumanPath() const { return m_szHumanPath; }	///< Human68kパス名を取得

private:
	DWORD m_nKey;						///< Human68kのFILESバッファアドレス 0なら未使用
	DWORD m_nHumanWildcard;					///< Human68kのワイルドカード情報
	DWORD m_nHumanAttribute;				///< Human68kの検索属性
	CHostPath::find_t m_findNext;				///< 次回検索位置情報
	Human68k::dirent_t m_dirHuman;				///< 検索結果 Human68kファイル情報
	BYTE m_szHumanFilename[24];				///< Human68kのファイル名
	BYTE m_szHumanResult[24];				///< 検索結果 Human68kファイル名
	BYTE m_szHumanPath[HUMAN68K_PATH_MAX];
								///< Human68kのパス名
	TCHAR m_szHostResult[FILEPATH_MAX];			///< 検索結果 ホスト側のフルパス名
};

//===========================================================================
//
/// ファイル検索領域 マネージャ
//
//===========================================================================
class CHostFilesManager {
public:
#ifdef _DEBUG
	// 基本ファンクション
	~CHostFilesManager();					///< デストラクタ final
#endif	// _DEBUG
	void  Init();						///< 初期化 (ドライバ組込み時)
	void  Clean();						///< 解放 (起動・リセット時)

	CHostFiles*  Alloc(DWORD nKey);				///< 確保
	CHostFiles*  Search(DWORD nKey);			///< 検索
	void  Free(CHostFiles* pFiles);				///< 解放
private:
	/// メモリ管理用
	struct ring_t {
		CRing r;					///< 円環
		CHostFiles f;					///< 実体
	};

	CRing m_cRing;						///< CHostFiles連結用
};

//===========================================================================
//
/// FCB処理
//
//===========================================================================
class CHostFcb {
public:
	// 基本ファンクション
	CHostFcb() { SetKey(0); Init(); }						///< デフォルトコンストラクタ
	~CHostFcb() { Close(); }							///< デストラクタ final
	void  Init();									///< 初期化

	void  SetKey(DWORD nKey) { m_nKey = nKey; }			///< 検索キー設定
	BOOL  isSameKey(DWORD nKey) const { return m_nKey == nKey; }	///< 検索キー比較
	void  SetUpdate() { m_bUpdate = TRUE; }				///< 更新
	BOOL  isUpdate() const { return m_bUpdate; }			///< 更新状態取得
	BOOL  SetMode(DWORD nHumanMode);						///< ファイルオープンモードを設定
	void  SetFilename(const TCHAR* szFilename);					///< ファイル名を設定
	void  SetHumanPath(const BYTE* szHumanPath);					///< Human68kパス名を設定
	const BYTE*  GetHumanPath() const { return m_szHumanPath; }	///< Human68kパス名を取得

	BOOL  Create(Human68k::fcb_t* pFcb, DWORD nHumanAttribute, BOOL bForce);	///< ファイル作成
	BOOL  Open();									///< ファイルオープン
	BOOL  Rewind(DWORD nOffset);							///< ファイルシーク
	DWORD  Read(BYTE* pBuffer, DWORD nSize);					///< ファイル読み込み
	DWORD  Write(const BYTE* pBuffer, DWORD nSize);					///< ファイル書き込み
	BOOL  Truncate();								///< ファイル切り詰め
	DWORD  Seek(DWORD nOffset, DWORD nHumanSeek);					///< ファイルシーク
	BOOL  TimeStamp(DWORD nHumanTime);						///< ファイル時刻設定
	BOOL  Close();									///< ファイルクローズ

private:
	DWORD m_nKey;									///< Human68kのFCBバッファアドレス (0なら未使用)
	BOOL m_bUpdate;									///< 更新フラグ
	FILE* m_pFile;									///< ホスト側のファイルオブジェクト
	const char* m_pszMode;								///< ホスト側のファイルオープンモード
	bool m_bFlag;									///< ホスト側のファイルオープンフラグ
	BYTE m_szHumanPath[HUMAN68K_PATH_MAX];
											///< Human68kのパス名
	TCHAR m_szFilename[FILEPATH_MAX];						///< ホスト側のファイル名
};

//===========================================================================
//
/// FCB処理 マネージャ
//
//===========================================================================
class CHostFcbManager {
public:
	#ifdef _DEBUG
	// 基本ファンクション
	~CHostFcbManager();							///< デストラクタ final
	#endif	// _DEBUG
	void  Init();								///< 初期化 (ドライバ組込み時)
	void  Clean();								///< 解放 (起動・リセット時)

	CHostFcb*  Alloc(DWORD nKey);						///< 確保
	CHostFcb*  Search(DWORD nKey);						///< 検索
	void  Free(CHostFcb* p);						///< 解放

private:
	/// メモリ管理用
	struct ring_t {
		CRing r;							///< 円環
		CHostFcb f;							///< 実体
	};

	CRing m_cRing;								///< CHostFcb連結用
};

//===========================================================================
//
/// ホスト側ドライブ
///
/// ドライブ毎に必要な情報の保持に専念し、管理はCHostEntryで行なう。
//
//===========================================================================
class CHostDrv
{
public:
	// 基本ファンクション
	CHostDrv();								///< デフォルトコンストラクタ
	~CHostDrv();								///< デストラクタ final
	void  Init(const TCHAR* szBase, DWORD nFlag);				///< 初期化 (デバイス起動とロード)

	BOOL  isWriteProtect() const { return m_bWriteProtect; }	///< 書き込み禁止か？
	BOOL  isEnable() const { return m_bEnable; }		///< アクセス可能か？
	BOOL  isMediaOffline();							///< メディアチェック
	BYTE  GetMediaByte() const;						///< メディアバイトの取得
	DWORD  GetStatus() const;						///< ドライブ状態の取得
	void  SetEnable(BOOL bEnable);						///< メディア状態設定
	BOOL CheckMedia();							///< メディア交換チェック
	void Update();								///< メディア状態更新
	void Eject();								///< イジェクト
	void  GetVolume(TCHAR* szLabel);					///< ボリュームラベルの取得
	BOOL  GetVolumeCache(TCHAR* szLabel) const;				///< キャッシュからボリュームラベルを取得
	DWORD  GetCapacity(Human68k::capacity_t* pCapacity);			///< 容量の取得
	BOOL  GetCapacityCache(Human68k::capacity_t* pCapacity) const;		///< キャッシュから容量を取得

	// キャッシュ操作
	void  CleanCache();							///< 全てのキャッシュを更新する
	void  CleanCache(const BYTE* szHumanPath);				///< 指定されたパスのキャッシュを更新する
	void  CleanCacheChild(const BYTE* szHumanPath);				///< 指定されたパス以下のキャッシュを全て更新する
	void  DeleteCache(const BYTE* szHumanPath);				///< 指定されたパスのキャッシュを削除する
	CHostPath*  FindCache(const BYTE* szHuman);				///< 指定されたパスがキャッシュされているか検索する
	CHostPath*  CopyCache(CHostFiles* pFiles);				///< キャッシュ情報を元に、ホスト側の名称を獲得する
	CHostPath*  MakeCache(CHostFiles* pFiles);				///< ホスト側の名称の構築に必要な情報をすべて取得する
	BOOL  Find(CHostFiles* pFiles);						///< ホスト側の名称を検索 (パス名+ファイル名(省略可)+属性)

private:
	// パス名操作
	static const BYTE*  SeparateCopyFilename(const BYTE* szHuman, BYTE* szBuffer);
										///< Human68kフルパス名から先頭の要素を分離・コピー

	// 排他制御
	void  Lock() {}
	void  Unlock() {}

	/// メモリ管理用
	struct ring_t {
		CRing r;						///< 円環
		CHostPath f;						///< 実体
	};

	BOOL m_bWriteProtect;						///< 書き込み禁止ならTRUE
	BOOL m_bEnable;							///< メディアが利用可能ならTRUE
	DWORD m_nRing;							///< パス名保持数
	CRing m_cRing;							///< CHostPath連結用
	Human68k::capacity_t m_capCache;				///< セクタ情報キャッシュ sectors == 0 なら未キャッシュ
	BOOL m_bVolumeCache;						///< ボリュームラベル読み込み済みならTRUE
	TCHAR m_szVolumeCache[24];					///< ボリュームラベルキャッシュ
	TCHAR m_szBase[FILEPATH_MAX];					///< ベースパス
};

//===========================================================================
//
/// ディレクトリエントリ管理
//
//===========================================================================
class CHostEntry {
public:
	// 基本ファンクション
	CHostEntry();								///< デフォルトコンストラクタ
	~CHostEntry();								///< デストラクタ final
	void  Init();								///< 初期化 (ドライバ組込み時)
	void  Clean();								///< 解放 (起動・リセット時)

	// キャッシュ操作
	void  CleanCache();							///< 全てのキャッシュを更新する
	void  CleanCache(DWORD nUnit);						///< 指定されたユニットのキャッシュを更新する
	void  CleanCache(DWORD nUnit, const BYTE* szHumanPath);			///< 指定されたパスのキャッシュを更新する
	void  CleanCacheChild(DWORD nUnit, const BYTE* szHumanPath);		///< 指定されたパス以下のキャッシュを全て更新する
	void  DeleteCache(DWORD nUnit, const BYTE* szHumanPath);		///< 指定されたパスのキャッシュを削除する
	BOOL  Find(DWORD nUnit, CHostFiles* pFiles);				///< ホスト側の名称を検索 (パス名+ファイル名(省略可)+属性)
	void  ShellNotify(DWORD nEvent, const TCHAR* szPath);			///< ホスト側ファイルシステム状態変化通知

	// ドライブオブジェクト操作
	void  SetDrv(DWORD nUnit, CHostDrv* pDrv);				///< ドライブ設定
	BOOL  isWriteProtect(DWORD nUnit) const;				///< 書き込み禁止か？
	BOOL  isEnable(DWORD nUnit) const;					///< アクセス可能か？
	BOOL  isMediaOffline(DWORD nUnit);					///< メディアチェック
	BYTE  GetMediaByte(DWORD nUnit) const;					///< メディアバイトの取得
	DWORD  GetStatus(DWORD nUnit) const;					///< ドライブ状態の取得
	BOOL CheckMedia(DWORD nUnit);						///< メディア交換チェック
	void Eject(DWORD nUnit);						///< イジェクト
	void  GetVolume(DWORD nUnit, TCHAR* szLabel);				///< ボリュームラベルの取得
	BOOL  GetVolumeCache(DWORD nUnit, TCHAR* szLabel) const;		///< キャッシュからボリュームラベルを取得
	DWORD  GetCapacity(DWORD nUnit, Human68k::capacity_t* pCapacity);	///< 容量の取得
	BOOL  GetCapacityCache(DWORD nUnit, Human68k::capacity_t* pCapacity) const;
										///< キャッシュからクラスタサイズを取得

	/// 定数
	enum {
		DriveMax = 10							///< ドライブ最大候補数
	};

private:
	CHostDrv* m_pDrv[DriveMax];						///< ホスト側ドライブオブジェクト
	DWORD m_nTimeout;							///< 最後にタイムアウトチェックを行なった時刻
};

//===========================================================================
//
/// ホスト側ファイルシステム
//
//===========================================================================
/** @note
現在の見解。

XM6の設計思想とは反するが、class Windrvまたはclass CWindrvに直接
class CFileSysへのポインタを持たせる方法を模索するべきである。
これにより、以下のメリットが得られる。

メリットその1。
コマンドハンドラの大量のメソッド群を一ヶ所で集中管理できる。
コマンドハンドラはホスト側の仕様変更などの要因によって激しく変化する
可能性が高いため、メソッドの追加削除や引数の変更などのメンテナンスが
大幅に楽になる。

メリットその2。
仮想関数のテーブル生成・参照処理に関する処理コードを駆逐できる。
XM6では複数のファイルシステムオブジェクトを同時に使うような実装は
ありえない。つまりファイルシステムオブジェクトにポリモーフィズムは
まったく必要ないどころか、ただクロックの無駄となっているだけである。

試しに変えてみた。実際効率上がった。
windrv.h内のFILESYS_FAST_STRUCTUREの値を変えてコンパイラの吐くソース
を比較すれば一目瞭然。何故私がこんな長文を書こうと思ったのかを理解で
きるはず。

一方ロシアはclass CWindrv内にclass CFileSysを直接設置した。
(本当はclass CHostを廃止して直接置きたい……良い方法はないものか……)
*/
class CFileSys
{
public:
	// 基本ファンクション
	CFileSys();								///< デフォルトコンストラクタ
	virtual ~CFileSys() {};							///< デストラクタ

	// 初期化・終了
	void Reset();								///< リセット (全クローズ)
	void Init();								///< 初期化 (デバイス起動とロード)

	// コマンドハンドラ
	DWORD InitDevice(const Human68k::argument_t* pArgument);		///< $40 - デバイス起動
	int CheckDir(DWORD nUnit, const Human68k::namests_t* pNamests);		///< $41 - ディレクトリチェック
	int MakeDir(DWORD nUnit, const Human68k::namests_t* pNamests);		///< $42 - ディレクトリ作成
	int RemoveDir(DWORD nUnit, const Human68k::namests_t* pNamests);	///< $43 - ディレクトリ削除
	int Rename(DWORD nUnit, const Human68k::namests_t* pNamests, const Human68k::namests_t* pNamestsNew);
										///< $44 - ファイル名変更
	int Delete(DWORD nUnit, const Human68k::namests_t* pNamests);		///< $45 - ファイル削除
	int Attribute(DWORD nUnit, const Human68k::namests_t* pNamests, DWORD nHumanAttribute);
										///< $46 - ファイル属性取得/設定
	int Files(DWORD nUnit, DWORD nKey, const Human68k::namests_t* pNamests, Human68k::files_t* pFiles);
										///< $47 - ファイル検索
	int NFiles(DWORD nUnit, DWORD nKey, Human68k::files_t* pFiles);		///< $48 - ファイル次検索
	int Create(DWORD nUnit, DWORD nKey, const Human68k::namests_t* pNamests, Human68k::fcb_t* pFcb, DWORD nHumanAttribute, BOOL bForce);
										///< $49 - ファイル作成
	int Open(DWORD nUnit, DWORD nKey, const Human68k::namests_t* pNamests, Human68k::fcb_t* pFcb);
										///< $4A - ファイルオープン
	int Close(DWORD nUnit, DWORD nKey, Human68k::fcb_t* pFcb);		///< $4B - ファイルクローズ
	int Read(DWORD nKey, Human68k::fcb_t* pFcb, BYTE* pAddress, DWORD nSize);
										///< $4C - ファイル読み込み
	int Write(DWORD nKey, Human68k::fcb_t* pFcb, const BYTE* pAddress, DWORD nSize);
										///< $4D - ファイル書き込み
	int Seek(DWORD nKey, Human68k::fcb_t* pFcb, DWORD nSeek, int nOffset);	///< $4E - ファイルシーク
	DWORD TimeStamp(DWORD nUnit, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nHumanTime);
										///< $4F - ファイル時刻取得/設定
	int GetCapacity(DWORD nUnit, Human68k::capacity_t* pCapacity);		///< $50 - 容量取得
	int CtrlDrive(DWORD nUnit, Human68k::ctrldrive_t* pCtrlDrive);		///< $51 - ドライブ状態検査/制御
	int GetDPB(DWORD nUnit, Human68k::dpb_t* pDpb);				///< $52 - DPB取得
	int DiskRead(DWORD nUnit, BYTE* pBuffer, DWORD nSector, DWORD nSize);	///< $53 - セクタ読み込み
	int DiskWrite(DWORD nUnit);						///< $54 - セクタ書き込み
	int Ioctrl(DWORD nUnit, DWORD nFunction, Human68k::ioctrl_t* pIoctrl);	///< $55 - IOCTRL
	int Flush(DWORD nUnit);							///< $56 - フラッシュ
	int CheckMedia(DWORD nUnit);						///< $57 - メディア交換チェック
	int Lock(DWORD nUnit);							///< $58 - 排他制御

	void SetOption(DWORD nOption);						///< オプション設定
	DWORD GetOption() const { return m_nOption; }		///< オプション取得
	DWORD GetDefault() const { return m_nOptionDefault; }	///< デフォルトオプション取得
	static DWORD GetFileOption() { return g_nOption; }			///< ファイル名変換オプション取得
	void ShellNotify(DWORD nEvent, const TCHAR* szPath)
	{ m_cEntry.ShellNotify(nEvent, szPath); }			///< ホスト側ファイルシステム状態変化通知

	/// 定数
	enum {
		DriveMax = CHostEntry::DriveMax					///< ドライブ最大候補数
	};

private:
	// 内部補助用
	void  InitOption(const Human68k::argument_t* pArgument);		///< オプション初期化
	BOOL  FilesVolume(DWORD nUnit, Human68k::files_t* pFiles);		///< ボリュームラベル取得

	DWORD m_nUnits;								///< 現在のドライブオブジェクト数 (レジューム毎に変化)

	DWORD m_nOption;							///< 現在の動作フラグ
	DWORD m_nOptionDefault;							///< リセット時の動作フラグ

	DWORD m_nDrives;							///< ベースパス状態復元用の候補数 (0なら毎回スキャン)

	DWORD m_nKernel;							///< カーネルチェック用カウンタ
	DWORD m_nKernelSearch;							///< NULデバイスの先頭アドレス

	DWORD m_nHostSectorCount;						///< 擬似セクタ番号

	CHostFilesManager m_cFiles;						///< ファイル検索領域
	CHostFcbManager m_cFcb;							///< FCB操作領域
	CHostEntry m_cEntry;							///< ドライブオブジェクトとディレクトリエントリ

	DWORD m_nHostSectorBuffer[XM6_HOST_PSEUDO_CLUSTER_MAX];
										///< 擬似セクタの指すファイル実体

	DWORD m_nFlag[DriveMax];						///< ベースパス状態復元用の動作フラグ候補
	TCHAR m_szBase[DriveMax][FILEPATH_MAX];					///< ベースパス状態復元用の候補
	static DWORD g_nOption;							///< ファイル名変換フラグ
};

#endif	// cfilesystem_h
