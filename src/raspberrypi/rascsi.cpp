//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ メイン ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"
#include "disk.h"
#include "gpiobus.h"

//---------------------------------------------------------------------------
//
//	定数宣言
//
//---------------------------------------------------------------------------
#define CtrlMax	8					// 最大SCSIコントローラ数
#define UnitNum	2					// コントローラ辺りのユニット数
#ifdef BAREMETAL
#define FPRT(fp, ...) printf( __VA_ARGS__ )
#else
#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )
#endif	// BAREMETAL

//---------------------------------------------------------------------------
//
//	変数宣言
//
//---------------------------------------------------------------------------
static volatile BOOL running;		// 実行中フラグ
static volatile BOOL active;		// 処理中フラグ
SASIDEV *ctrl[CtrlMax];				// コントローラ
Disk *disk[CtrlMax * UnitNum];		// ディスク
GPIOBUS *bus;						// GPIOバス
#ifdef BAREMETAL
FATFS fatfs;						// FatFS
#else
int monsocket;						// モニター用ソケット
pthread_t monthread;				// モニタースレッド
static void *MonThread(void *param);
#endif	// BAREMETAL

#ifndef BAREMETAL
//---------------------------------------------------------------------------
//
//	シグナル処理
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
	// 停止指示
	running = FALSE;
}
#endif	// BAREMETAL

//---------------------------------------------------------------------------
//
//	バナー出力
//
//---------------------------------------------------------------------------
void Banner(int argc, char* argv[])
{
	FPRT(stdout,"SCSI Target Emulator RaSCSI(*^..^*) ");
	FPRT(stdout,"version %01d.%01d%01d(%s, %s)\n",
		(int)((VERSION >> 8) & 0xf),
		(int)((VERSION >> 4) & 0xf),
		(int)((VERSION     ) & 0xf),
		__DATE__,
		__TIME__);
	FPRT(stdout,"Powered by XM6 TypeG Technology / ");
	FPRT(stdout,"Copyright (C) 2016-2020 GIMONS\n");
	FPRT(stdout,"Connect type : %s\n", CONNECT_DESC);

	if (argc > 1 && strcmp(argv[1], "-h") == 0) {
		FPRT(stdout,"\n");
		FPRT(stdout,"Usage: %s [-IDn FILE] ...\n\n", argv[0]);
		FPRT(stdout," n is SCSI identification number(0-7).\n");
		FPRT(stdout," FILE is disk image file.\n\n");
		FPRT(stdout,"Usage: %s [-HDn FILE] ...\n\n", argv[0]);
		FPRT(stdout," n is X68000 SASI HD number(0-15).\n");
		FPRT(stdout," FILE is disk image file.\n\n");
		FPRT(stdout," Image type is detected based on file extension.\n");
		FPRT(stdout,"  hdf : SASI HD image(XM6 SASI HD image)\n");
		FPRT(stdout,"  hds : SCSI HD image(XM6 SCSI HD image)\n");
		FPRT(stdout,"  hdn : SCSI HD image(NEC GENUINE)\n");
		FPRT(stdout,"  hdi : SCSI HD image(Anex86 HD image)\n");
		FPRT(stdout,"  nhd : SCSI HD image(T98Next HD image)\n");
		FPRT(stdout,"  hda : SCSI HD image(APPLE GENUINE)\n");
		FPRT(stdout,"  mos : SCSI MO image(XM6 SCSI MO image)\n");
		FPRT(stdout,"  iso : SCSI CD image(ISO 9660 image)\n");

#ifndef BAREMETAL
		exit(0);
#endif	// BAREMETAL
	}
}

//---------------------------------------------------------------------------
//
//	初期化
//
//---------------------------------------------------------------------------
BOOL Init()
{
	int i;

#ifndef BAREMETAL
	struct sockaddr_in server;
	int yes;

	// モニター用ソケット生成
	monsocket = socket(PF_INET, SOCK_STREAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = PF_INET;
	server.sin_port   = htons(6868);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	// アドレスの再利用を許可する
	yes = 1;
	if (setsockopt(
		monsocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
		return FALSE;
	}

	// バインド
	if (bind(monsocket, (struct sockaddr *)&server,
		sizeof(struct sockaddr_in)) < 0) {
		FPRT(stderr, "Error : Already running?\n");
		return FALSE;
	}

	// モニタースレッド生成
	pthread_create(&monthread, NULL, MonThread, NULL);

	// 割り込みハンドラ設定
	if (signal(SIGINT, KillHandler) == SIG_ERR) {
		return FALSE;
	}
	if (signal(SIGHUP, KillHandler) == SIG_ERR) {
		return FALSE;
	}
	if (signal(SIGTERM, KillHandler) == SIG_ERR) {
		return FALSE;
	}
#endif // BAREMETAL

	// GPIOBUS生成
	bus = new GPIOBUS();
	
	// GPIO初期化
	if (!bus->Init()) {
		return FALSE;
	}

	// バスリセット
	bus->Reset();

	// コントローラ初期化
	for (i = 0; i < CtrlMax; i++) {
		ctrl[i] = NULL;
	}

	// ディスク初期化
	for (i = 0; i < CtrlMax; i++) {
		disk[i] = NULL;
	}

	// その他
	running = FALSE;
	active = FALSE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	クリーンアップ
//
//---------------------------------------------------------------------------
void Cleanup()
{
	int i;

	// ディスク削除
	for (i = 0; i < CtrlMax * UnitNum; i++) {
		if (disk[i]) {
			delete disk[i];
			disk[i] = NULL;
		}
	}

	// コントローラ削除
	for (i = 0; i < CtrlMax; i++) {
		if (ctrl[i]) {
			delete ctrl[i];
			ctrl[i] = NULL;
		}
	}

	// バスをクリーンアップ
	bus->Cleanup();
	
	// GPIOBUS破棄
	delete bus;

#ifndef BAREMETAL
	// モニター用ソケットクローズ
	if (monsocket >= 0) {
		close(monsocket);
	}
#endif // BAREMETAL
}

//---------------------------------------------------------------------------
//
//	リセット
//
//---------------------------------------------------------------------------
void Reset()
{
	int i;

	// コントローラリセット
	for (i = 0; i < CtrlMax; i++) {
		if (ctrl[i]) {
			ctrl[i]->Reset();
		}
	}

	// バス信号線をリセット
	bus->Reset();
}

//---------------------------------------------------------------------------
//
//	デバイス一覧表示
//
//---------------------------------------------------------------------------
void ListDevice(FILE *fp)
{
	int i;
	int id;
	int un;
	Disk *pUnit;
	Filepath filepath;
	BOOL find;
	char type[5];

	find = FALSE;
	type[4] = 0;
	for (i = 0; i < CtrlMax * UnitNum; i++) {
		// IDとユニット
		id = i / UnitNum;
		un = i % UnitNum;
		pUnit = disk[i];

		// ユニットが存在しないまたはヌルディスクならスキップ
		if (pUnit == NULL || pUnit->IsNULL()) {
			continue;
		}

		// ヘッダー出力
		if (!find) {
			FPRT(fp, "\n");
			FPRT(fp, "+----+----+------+-------------------------------------\n");
			FPRT(fp, "| ID | UN | TYPE | DEVICE STATUS\n");
			FPRT(fp, "+----+----+------+-------------------------------------\n");
			find = TRUE;
		}

		// ID,UNIT,タイプ出力
		type[0] = (char)(pUnit->GetID() >> 24);
		type[1] = (char)(pUnit->GetID() >> 16);
		type[2] = (char)(pUnit->GetID() >> 8);
		type[3] = (char)(pUnit->GetID());
		FPRT(fp, "|  %d |  %d | %s | ", id, un, type);

		// マウント状態出力
		if (pUnit->GetID() == MAKEID('S', 'C', 'B', 'R')) {
			FPRT(fp, "%s", "HOST BRIDGE");
		} else {
			pUnit->GetPath(filepath);
			FPRT(fp, "%s",
				(pUnit->IsRemovable() && !pUnit->IsReady()) ?
				"NO MEDIA" : filepath.GetPath());
		}

		// ライトプロテクト状態出力
		if (pUnit->IsRemovable() && pUnit->IsReady() && pUnit->IsWriteP()) {
			FPRT(fp, "(WRITEPROTECT)");
		}

		// 次の行へ
		FPRT(fp, "\n");
	}

	// コントローラが無い場合
	if (!find) {
		FPRT(fp, "No device is installed.\n");
		return;
	}
	
	FPRT(fp, "+----+----+------+-------------------------------------\n");
}

//---------------------------------------------------------------------------
//
//	コントローラマッピング
//
//---------------------------------------------------------------------------
void MapControler(FILE *fp, Disk **map)
{
	int i;
	int j;
	int unitno;
	int sasi_num;
	int scsi_num;

	// 変更されたユニットの置き換え
	for (i = 0; i < CtrlMax; i++) {
		for (j = 0; j < UnitNum; j++) {
			unitno = i * UnitNum + j;
			if (disk[unitno] != map[unitno]) {
				// 元のユニットが存在する
				if (disk[unitno]) {
					// コントローラから切り離す
					if (ctrl[i]) {
						ctrl[i]->SetUnit(j, NULL);
					}

					// ユニットを解放
					delete disk[unitno];
				}

				// 新しいユニットの設定
				disk[unitno] = map[unitno];
			}
		}
	}

	// 全コントローラを再構成
	for (i = 0; i < CtrlMax; i++) {
		// ユニット構成を調べる
		sasi_num = 0;
		scsi_num = 0;
		for (j = 0; j < UnitNum; j++) {
			unitno = i * UnitNum + j;
			// ユニットの種類で分岐
			if (disk[unitno]) {
				if (disk[unitno]->IsSASI()) {
					// SASIドライブ数加算
					sasi_num++;
				} else {
					// SCSIドライブ数加算
					scsi_num++;
				}
			}

			// ユニットを取り外す
			if (ctrl[i]) {
				ctrl[i]->SetUnit(j, NULL);
			}
		}

		// 接続ユニットなし
		if (sasi_num == 0 && scsi_num == 0) {
			if (ctrl[i]) {
				delete ctrl[i];
				ctrl[i] = NULL;
				continue;
			}
		}

		// SASI,SCSI混在
		if (sasi_num > 0 && scsi_num > 0) {
			FPRT(fp, "Error : SASI and SCSI can't be mixed\n");
			continue;
		}

		if (sasi_num > 0) {
			// SASIのユニットのみ

			// コントローラのタイプが違うなら解放
			if (ctrl[i] && !ctrl[i]->IsSASI()) {
				delete ctrl[i];
				ctrl[i] = NULL;
			}

			// SASIコントローラ生成
			if (!ctrl[i]) {
				ctrl[i] = new SASIDEV();
				ctrl[i]->Connect(i, bus);
			}
		} else {
			// SCSIのユニットのみ

			// コントローラのタイプが違うなら解放
			if (ctrl[i] && !ctrl[i]->IsSCSI()) {
				delete ctrl[i];
				ctrl[i] = NULL;
			}

			// SCSIコントローラ生成
			if (!ctrl[i]) {
				ctrl[i] = new SCSIDEV();
				ctrl[i]->Connect(i, bus);
			}
		}

		// 全ユニット接続
		for (j = 0; j < UnitNum; j++) {
			unitno = i * UnitNum + j;
			if (disk[unitno]) {
				// ユニット接続
				ctrl[i]->SetUnit(j, disk[unitno]);
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	コマンド処理
//
//---------------------------------------------------------------------------
BOOL ProcessCmd(FILE *fp, int id, int un, int cmd, int type, char *file)
{
	Disk *map[CtrlMax * UnitNum];
	int len;
	char *ext;
	Filepath filepath;
	Disk *pUnit;

	// ユニット一覧を複写
	memcpy(map, disk, sizeof(disk));

	// IDチェック
	if (id < 0 || id >= CtrlMax) {
		FPRT(fp, "Error : Invalid ID\n");
		return FALSE;
	}

	// ユニットチェック
	if (un < 0 || un >= UnitNum) {
		FPRT(fp, "Error : Invalid unit number\n");
		return FALSE;
	}

	// 接続コマンド
	if (cmd == 0) {					// ATTACH
		// SASIとSCSIを見分ける
		ext = NULL;
		pUnit = NULL;
		if (type == 0) {
			// パスチェック
			if (!file) {
				return FALSE;
			}

			// 最低5文字
			len = strlen(file);
			if (len < 5) {
				return FALSE;
			}

			// 拡張子チェック
			if (file[len - 4] != '.') {
				return FALSE;
			}

			// 拡張子がSASIタイプで無ければSCSIに差し替え
			ext = &file[len - 3];
			if (xstrcasecmp(ext, "hdf") != 0) {
				type = 1;
			}
		}

		// タイプ別のインスタンスを生成
		switch (type) {
			case 0:		// HDF
				pUnit = new SASIHD();
				break;
			case 1:		// HDS/HDN/HDI/NHD/HDA
				if (ext == NULL) {
					break;
				}
				if (xstrcasecmp(ext, "hdn") == 0 ||
					xstrcasecmp(ext, "hdi") == 0 || xstrcasecmp(ext, "nhd") == 0) {
					pUnit = new SCSIHD_NEC();
				} else if (xstrcasecmp(ext, "hda") == 0) {
					pUnit = new SCSIHD_APPLE();
				} else {
					pUnit = new SCSIHD();
				}
				break;
			case 2:		// MO
				pUnit = new SCSIMO();
				break;
			case 3:		// CD
				pUnit = new SCSICD();
				break;
			case 4:		// BRIDGE
				pUnit = new SCSIBR();
				break;
			default:
				FPRT(fp,	"Error : Invalid device type\n");
				return FALSE;
		}

		// ドライブはファイルの確認を行う
		if (type <= 1 || (type <= 3 && xstrcasecmp(file, "-") != 0)) {
			// パスを設定
			filepath.SetPath(file);

			// オープン
			if (!pUnit->Open(filepath)) {
				FPRT(fp, "Error : File open error [%s]\n", file);
				delete pUnit;
				return FALSE;
			}
		}

		// ライトスルーに設定
		pUnit->SetCacheWB(FALSE);

		// 新しいユニットで置き換え
		map[id * UnitNum + un] = pUnit;

		// コントローラマッピング
		MapControler(fp, map);
		return TRUE;
	}

	// 有効なコマンドか
	if (cmd > 4) {
		FPRT(fp, "Error : Invalid command\n");
		return FALSE;
	}

	// コントローラが存在するか
	if (ctrl[id] == NULL) {
		FPRT(fp, "Error : No such device\n");
		return FALSE;
	}

	// ユニットが存在するか
	pUnit = disk[id * UnitNum + un];
	if (pUnit == NULL) {
		FPRT(fp, "Error : No such device\n");
		return FALSE;
	}

	// 切断コマンド
	if (cmd == 1) {					// DETACH
		// 既存のユニットを解放
		map[id * UnitNum + un] = NULL;

		// コントローラマッピング
		MapControler(fp, map);
		return TRUE;
	}

	// MOかCDの場合だけ有効
	if (pUnit->GetID() != MAKEID('S', 'C', 'M', 'O') &&
		pUnit->GetID() != MAKEID('S', 'C', 'C', 'D')) {
		FPRT(fp, "Error : Operation denied(Deveice isn't removable)\n");
		return FALSE;
	}

	switch (cmd) {
		case 2:						// INSERT
			// パスを設定
			filepath.SetPath(file);

			// オープン
			if (pUnit->Open(filepath)) {
				FPRT(fp, "Error : File open error [%s]\n", file);
				return FALSE;
			}
			break;

		case 3:						// EJECT
			pUnit->Eject(TRUE);
			break;

		case 4:						// PROTECT
			if (pUnit->GetID() != MAKEID('S', 'C', 'M', 'O')) {
				FPRT(fp, "Error : Operation denied(Deveice isn't MO)\n");
				return FALSE;
			}
			pUnit->WriteP(!pUnit->IsWriteP());
			break;
		default:
			ASSERT(FALSE);
			return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	引数処理
//
//---------------------------------------------------------------------------
BOOL ParseArgument(int argc, char* argv[])
{
#ifdef BAREMETAL
	FRESULT fr;
	FIL fp;
	char line[512];
#else
	int i;
#endif	// BAREMETAL
	int id;
	int un;
	int type;
	char *argID;
	char *argPath;
	int len;
	char *ext;

#ifdef BAREMETAL
	// SDカードマウント
	fr = f_mount(&fatfs, "", 1);
	if (fr != FR_OK) {
		FPRT(stderr, "Error : SD card mount failed.\n");
		return FALSE;
	}

	// 設定ファイルがなければ処理を中断
	fr = f_open(&fp, "rascsi.ini", FA_READ);
	if (fr != FR_OK) {
		return FALSE;
	}
#else
	// IDとパス指定がなければ処理を中断
	if (argc < 3) {
		return TRUE;
	}
	i = 1;
	argc--;
#endif	// BAREMETAL

	// 解読開始

	while (TRUE) {
#ifdef BAREMETAL
		// 1行取得
		memset(line, 0x00, sizeof(line));
		if (f_gets(line, sizeof(line) -1, &fp) == NULL) {
			break;
		}

		// CR/LF削除
		len = strlen(line);
		while (len > 0) {
			if (line[len - 1] != '\r' && line[len - 1] != '\n') {
				break;
			}
			line[len - 1] = '\0';
			len--;
		}
#else
		if (argc < 2) {
			break;
		}

		argc -= 2;
#endif	// BAREMETAL

		// IDとパスを取得
#ifdef BAREMETAL
		argID = &line[0];
		argPath = &line[4];
		line[3] = '\0';

		// 事前チェック
		if (argID[0] == '\0' || argPath[0] == '\0') {
			continue;
		}
#else
		argID = argv[i++];
		argPath = argv[i++];

		// 事前チェック
		if (argID[0] != '-') {
			FPRT(stderr,
				"Error : Invalid argument(-IDn or -HDn) [%s]\n", argID);
			goto parse_error;
		}
		argID++;
#endif	// BAREMETAL

		if (strlen(argID) == 3 && xstrncasecmp(argID, "id", 2) == 0) {
			// ID or idの形式

			// ID番号をチェック(0-7)
			if (argID[2] < '0' || argID[2] > '7') {
				FPRT(stderr,
					"Error : Invalid argument(IDn n=0-7) [%c]\n", argID[2]);
				goto parse_error;
			}

			// ID,ユニット確定
			id = argID[2] - '0';
			un = 0;
		} else if (xstrncasecmp(argID, "hd", 2) == 0) {
			// HD or hdの形式

			if (strlen(argID) == 3) {
				// HD番号をチェック(0-9)
				if (argID[2] < '0' || argID[2] > '9') {
					FPRT(stderr,
						"Error : Invalid argument(HDn n=0-15) [%c]\n", argID[2]);
					goto parse_error;
				}

				// ID,ユニット確定
				id = (argID[2] - '0') / UnitNum;
				un = (argID[2] - '0') % UnitNum;
			} else if (strlen(argID) == 4) {
				// HD番号をチェック(10-15)
				if (argID[2] != '1' || argID[3] < '0' || argID[3] > '5') {
					FPRT(stderr,
						"Error : Invalid argument(HDn n=0-15) [%c]\n", argID[2]);
					goto parse_error;
				}

				// ID,ユニット確定
				id = ((argID[3] - '0') + 10) / UnitNum;
				un = ((argID[3] - '0') + 10) % UnitNum;
#ifdef BAREMETAL
				argPath++;
#endif	// BAREMETAL
			} else {
				FPRT(stderr,
					"Error : Invalid argument(IDn or HDn) [%s]\n", argID);
				goto parse_error;
			}
		} else {
			FPRT(stderr,
				"Error : Invalid argument(IDn or HDn) [%s]\n", argID);
			goto parse_error;
		}

		// すでにアクティブなデバイスがあるならスキップ
		if (disk[id * UnitNum + un] &&
			!disk[id * UnitNum + un]->IsNULL()) {
			continue;
		}

		// デバイスタイプを初期化
		type = -1;

		// イーサネットとホストブリッジのチェック
		if (xstrcasecmp(argPath, "bridge") == 0) {
			type = 4;
		} else {
			// パスの長さをチェック
			len = strlen(argPath);
			if (len < 5) {
				FPRT(stderr,
					"Error : Invalid argument(File path is short) [%s]\n",
					argPath);
				goto parse_error;
			}

			// 拡張子を持っているか？
			if (argPath[len - 4] != '.') {
				FPRT(stderr,
					"Error : Invalid argument(No extension) [%s]\n", argPath);
				goto parse_error;
			}

			// タイプを決める
			ext = &argPath[len - 3];
			if (xstrcasecmp(ext, "hdf") == 0 ||
				xstrcasecmp(ext, "hds") == 0 ||
				xstrcasecmp(ext, "hdn") == 0 ||
				xstrcasecmp(ext, "hdi") == 0 || xstrcasecmp(ext, "nhd") == 0 ||
				xstrcasecmp(ext, "hda") == 0) {
				// HD(SASI/SCSI)
				type = 0;
			} else if (strcasecmp(ext, "mos") == 0) {
				// MO
				type = 2;
			} else if (strcasecmp(ext, "iso") == 0) {
				// CD
				type = 3;
			} else {
				// タイプが判別できない
				FPRT(stderr,
					"Error : Invalid argument(file type) [%s]\n", ext);
				goto parse_error;
			}
		}

		// コマンド実行
		if (!ProcessCmd(stderr, id, un, 0, type, argPath)) {
			goto parse_error;
		}
	}

#ifdef BAREMETAL
	// 設定ファイルクローズ
	f_close(&fp);
#endif	// BAREMETAL

	// デバイスリスト表示
	ListDevice(stdout);

	return TRUE;

parse_error:

#ifdef BAREMETAL
	// 設定ファイルクローズ
	f_close(&fp);
#endif	// BAREMETAL

	return FALSE;
}

#ifndef BAREMETAL
//---------------------------------------------------------------------------
//
//	スレッドを特定のCPUに固定する
//
//---------------------------------------------------------------------------
void FixCpu(int cpu)
{
	cpu_set_t cpuset;
	int cpus;

	// CPU数取得
	CPU_ZERO(&cpuset);
	sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
	cpus = CPU_COUNT(&cpuset);

	// アフィニティ設定
	if (cpu < cpus) {
		CPU_ZERO(&cpuset);
		CPU_SET(cpu, &cpuset);
		sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	}
}

//---------------------------------------------------------------------------
//
//	モニタースレッド
//
//---------------------------------------------------------------------------
static void *MonThread(void *param)
{
	struct sched_param schedparam;
	struct sockaddr_in client;
	socklen_t len; 
	int fd;
	FILE *fp;
	char buf[BUFSIZ];
	char *p;
	int i;
	char *argv[5];
	int id;
	int un;
	int cmd;
	int type;
	char *file;

	// スケジューラ設定
	schedparam.sched_priority = 0;
	sched_setscheduler(0, SCHED_IDLE, &schedparam);

	// CPUを固定
	FixCpu(2);

	// 実行開始待ち
	while (!running) {
		usleep(1);
	}

	// 監視準備
	listen(monsocket, 1);

	while (1) {
		// 接続待ち
		memset(&client, 0, sizeof(client)); 
		len = sizeof(client); 
		fd = accept(monsocket, (struct sockaddr*)&client, &len);
		if (fd < 0) {
			break;
		}

		// コマンド取得
		fp = fdopen(fd, "r+");
		p = fgets(buf, BUFSIZ, fp);

		// コマンド取得に失敗
		if (!p) {
			goto next;
		}

		// 改行文字を削除
		p[strlen(p) - 1] = 0;

		// デバイスリスト表示
		if (xstrncasecmp(p, "list", 4) == 0) {
			ListDevice(fp);
			goto next;
		}

		// パラメータの分離
		argv[0] = p;
		for (i = 1; i < 5; i++) {
			// パラメータ値をスキップ
			while (*p && (*p != ' ')) {
				p++;
			}

			// スペースをNULLに置換
			while (*p && (*p == ' ')) {
				*p++ = 0;
			}

			// パラメータを見失った
			if (!*p) {
				break;
			}

			// パラメータとして認識
			argv[i] = p;
		}

		// 全パラメータの取得失敗
		if (i < 5) {
			goto next;
		}

		// ID,ユニット,コマンド,タイプ,ファイル
		id = atoi(argv[0]);
		un = atoi(argv[1]);
		cmd = atoi(argv[2]);
		type = atoi(argv[3]);
		file = argv[4];

		// アイドルになるまで待つ
		while (active) {
			usleep(500 * 1000);
		}

		// コマンド実行
		ProcessCmd(fp, id, un, cmd, type, file);

next:
		// 接続解放
		fclose(fp);
		close(fd);
	}

	return NULL;
}
#endif	// BAREMETAL

//---------------------------------------------------------------------------
//
//	主処理
//
//---------------------------------------------------------------------------
#ifdef BAREMETAL
extern "C"
int startrascsi(void)
{
	int argc = 0;
	char** argv = NULL;
#else
int main(int argc, char* argv[])
{
#endif	// BAREMETAL
	int i;
	int ret;
	int actid;
	DWORD now;
	BUS::phase_t phase;
	BYTE data;
#ifndef BAREMETAL
	struct sched_param schparam;
#endif	// BAREMETAL

	// バナー出力
	Banner(argc, argv);

	// 初期化
	ret = 0;
	if (!Init()) {
		ret = EPERM;
		goto init_exit;
	}

	// リセット
	Reset();

#ifdef BAREMETAL
	// BUSYアサート(ホスト側を待たせるため)
	bus->SetBSY(TRUE);
#endif

	// 引数処理
	if (!ParseArgument(argc, argv)) {
		ret = EINVAL;
		goto err_exit;
	}

#ifdef BAREMETAL
	// BUSYネゲート(ホスト側を待たせるため)
	bus->SetBSY(FALSE);
#endif

#ifndef BAREMETAL
	// CPUを固定
	FixCpu(3);

#ifdef USE_SEL_EVENT_ENABLE
	// スケジューリングポリシー設定(最優先)
	schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// USE_SEL_EVENT_ENABLE
#endif	// BAREMETAL

	// 実行開始
	running = TRUE;

	// メインループ
	while (running) {
		// ワーク初期化
		actid = -1;
		phase = BUS::busfree;

#ifdef USE_SEL_EVENT_ENABLE
		// SEL信号ポーリング
		if (bus->PollSelectEvent() < 0) {
			// 割り込みで停止
			if (errno == EINTR) {
				break;
			}
			continue;
		}

		// バスの状態取得
		bus->Aquire();
#else
		bus->Aquire();
		if (!bus->GetSEL()) {
#if !defined(BAREMETAL)
			usleep(0);
#endif	// !BAREMETAL
			continue;
		}
#endif	// USE_SEL_EVENT_ENABLE

		// イニシエータがID設定中にアサートしている
		// 可能性があるのでBSYが解除されるまで待つ(最大3秒)
		if (bus->GetBSY()) {
			now = SysTimer::GetTimerLow();
			while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000) {
				bus->Aquire();
				if (!bus->GetBSY()) {
					break;
				}
			}
		}

		// ビジー、または他のデバイスが応答したので止める
		if (bus->GetBSY() || !bus->GetSEL()) {
			continue;
		}

		// 全コントローラに通知
		data = bus->GetDAT();
		for (i = 0; i < CtrlMax; i++) {
			if (!ctrl[i] || (data & (1 << i)) == 0) {
				continue;
			}

			// セレクションフェーズに移行したターゲットを探す
			if (ctrl[i]->Process() == BUS::selection) {
				// ターゲットのIDを取得
				actid = i;

				// セレクションフェーズ
				phase = BUS::selection;
				break;
			}
		}

		// セレクションフェーズが開始されていなければバスの監視へ戻る
		if (phase != BUS::selection) {
			continue;
		}

		// ターゲット走行開始
		active = TRUE;

#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
		// スケジューリングポリシー設定(最優先)
		schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
		sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// !USE_SEL_EVENT_ENABLE && !BAREMETAL

		// バスフリーになるまでループ
		while (running) {
			// ターゲット駆動
			phase = ctrl[actid]->Process();

			// バスフリーになったら終了
			if (phase == BUS::busfree) {
				break;
			}
		}

#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
		// スケジューリングポリシー設定(ノーマル)
		schparam.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schparam);
#endif	// !USE_SEL_EVENT_ENABLE && !BAREMETAL

		// ターゲット走行終了
		active = FALSE;
	}

err_exit:
	// クリーンアップ
	Cleanup();

init_exit:
#if !defined(BAREMETAL)
	exit(ret);
#else
	return ret;
#endif	// BAREMETAL
}
