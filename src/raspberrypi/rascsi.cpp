//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2018 GIMONS
//	[ メイン ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "fileio.h"
#include "filepath.h"
#include "disk.h"
#include "gpiobus.h"

//---------------------------------------------------------------------------
//
//	変数宣言
//
//---------------------------------------------------------------------------
enum {
	CtrlMax = 8,					// 最大SCSIコントローラ数
};

SASIDEV *ctrl[CtrlMax];				// コントローラ
Disk *disk[CtrlMax];				// ディスク
Filepath image[CtrlMax];			// イメージファイルパス
GPIOBUS bus;						// バス
BUS::phase_t phase;					// フェーズ
int actid;							// アクティブなコントローラID
volatile BOOL bRun;					// 実行中フラグ

BOOL bServer;						// サーバーモードフラグ
int monfd;							// モニター用ソケットFD
pthread_t monthread;				// モニタースレッド
static void *MonThread(void *param);

//---------------------------------------------------------------------------
//
//	シグナル処理
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
	// 停止指示
	bRun = FALSE;
}

//---------------------------------------------------------------------------
//
//	バナー出力
//
//---------------------------------------------------------------------------
BOOL Banner(int argc, char* argv[])
{
	printf("SCSI Target Emulator RaSCSI(*^..^*) ");
	printf("version %01d.%01d%01d\n",
		(int)((VERSION >> 8) & 0xf),
		(int)((VERSION >> 4) & 0xf),
		(int)((VERSION     ) & 0xf));
	printf("Powered by XM6 TypeG Technology / ");
	printf("Copyright (C) 2016-2018 GIMONS\n");
	printf("Connect type : %s\n", CONNECT_DESC);

	if (argc > 1 && strcmp(argv[1], "-h") == 0) {
		printf("\n");
		printf("Usage: %s [-ID{0|1|2|3|4|5|6|7} FILE] ...\n\n", argv[0]);
		printf(" IDn is SCSI identification number.\n");
		printf(" FILE is disk image file.\n\n");
		printf(" Detected images type based on file extension.\n");
		printf("  hdf : SASI HD image(XM6 SASI HD image)\n");
		printf("  hds : SCSI HD image(XM6 SCSI HD image)\n");
		printf("  hdn : SCSI HD image(NEC GENUINE)\n");
		printf("  hdi : SCSI HD image(Anex86 HD image)\n");
		printf("  nhd : SCSI HD image(T98Next HD image)\n");
		printf("  hda : SCSI HD image(APPLE GENUINE)\n");
		printf("  mos : SCSI MO image(XM6 SCSI MO image)\n");
		printf("  iso : SCSI CD image(ISO 9660 image)\n");
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	初期化
//
//---------------------------------------------------------------------------
BOOL Init()
{
	int i;
	struct sockaddr_in server;

	// モニター用ソケット生成
	monfd = socket(PF_INET, SOCK_STREAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = PF_INET;
	server.sin_port   = htons(6868);
	server.sin_addr.s_addr = htonl(INADDR_ANY); 
	if (bind(monfd, (struct sockaddr *)&server,
		sizeof(struct sockaddr_in)) < 0) {
		if (errno != EADDRINUSE) {
			return FALSE;
		}
		bServer = FALSE;
		return TRUE;
	} else {
		bServer = TRUE;
	}

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

	// GPIO初期化
	if (!bus.Init()) {
		return FALSE;
	}

	// コントローラ初期化
	for (i = 0; i < CtrlMax; i++) {
		ctrl[i] = NULL;
	}

	// ディスク初期化
	for (i = 0; i < CtrlMax; i++) {
		disk[i] = NULL;
	}

	// イメージパス初期化
	for (i = 0; i < CtrlMax; i++) {
		image[i].Clear();
	}

	// アクティブID初期化
	actid = -1;

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
	for (i = 0; i < CtrlMax; i++) {
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
	bus.Cleanup();

	// モニター用ソケットクローズ
	if (monfd >= 0) {
		close(monfd);
	}
}

//---------------------------------------------------------------------------
//
//	リセット
//
//---------------------------------------------------------------------------
void Reset()
{
	int i;

	// アクティブID初期化
	actid = -1;

	// コントローラリセット
	for (i = 0; i < CtrlMax; i++) {
		if (ctrl[i]) {
			ctrl[i]->Reset();
		}
	}

	// バス信号線をリセット
	bus.Reset();
}

//---------------------------------------------------------------------------
//
//	デバイス一覧表示
//
//---------------------------------------------------------------------------
void ListDevice(FILE *fp)
{
	int i;
	Filepath filepath;
	BOOL find;
	char type[5];

	find = FALSE;
	type[4] = 0;
	for (i = 0; i < 8; i++) {
		if (disk[i] == NULL) {
			continue;
		}

		// ヘッダー出力
		if (!find) {
			fprintf(fp, "\n");
			fprintf(fp, "---+------+---------------------------------------\n");
			fprintf(fp, "ID | TYPE | DEVICE STATUS\n");
			fprintf(fp, "---+------+---------------------------------------\n");
			find = TRUE;
		}

		// ID,タイプ出力
		type[0] = (char)(disk[i]->GetID() >> 24);
		type[1] = (char)(disk[i]->GetID() >> 16);
		type[2] = (char)(disk[i]->GetID() >> 8);
		type[3] = (char)(disk[i]->GetID());
		fprintf(fp, " %d | %s | ", i, type);

		// マウント状態出力
		if (disk[i]->GetID() == MAKEID('S', 'C', 'B', 'R')) {
			fprintf(fp, "%s", "RaSCSI BRIDGE");
		} else {
			disk[i]->GetPath(filepath);
			fprintf(fp, "%s",
				(disk[i]->IsRemovable() && !disk[i]->IsReady()) ?
				"NO MEDIA" : filepath.GetPath());
		}

		// ライトプロテクト状態出力
		if (disk[i]->IsRemovable() && disk[i]->IsReady() && disk[i]->IsWriteP()) {
			fprintf(fp, "(WRITEPROTECT)");
		}

		// 次の行へ
		fprintf(fp, "\n");
	}

	// コントローラが無い場合
	if (!find) {
		fprintf(fp, "No device is installed.\n");
		return;
	}
	
	fprintf(fp, "---+------+---------------------------------------\n");
}

//---------------------------------------------------------------------------
//
//	コマンド処理
//
//---------------------------------------------------------------------------
BOOL ProcessCmd(FILE *fp, int id, int cmd, int type, char *file)
{
	int len;
	char *ext;
	Filepath filepath;

	// IDチェック
	if (id < 0 || id > 7) {
		fprintf(fp, "Error : Invalid ID\n");
		return FALSE;
	}

	// 接続コマンド
	if (cmd == 0) {					// ATTACH
		// コントローラを解放
		if (ctrl[id]) {
			ctrl[id]->SetUnit(0, NULL);
			delete ctrl[id];
			ctrl[id] = NULL;
		}

		// ディスクを解放
		if (disk[id]) {
			delete disk[id];
			disk[id] = NULL;
		}

		// SASIとSCSIを見分ける
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
				disk[id] = new SASIHD();
				break;
			case 1:		// HDS/HDN/HDI/NHD/HDA
				if (xstrcasecmp(ext, "hdn") == 0 ||
					xstrcasecmp(ext, "hdi") == 0 || xstrcasecmp(ext, "nhd") == 0) {
					disk[id] = new SCSIHD_NEC();
				} else if (xstrcasecmp(ext, "hda") == 0) {
					disk[id] = new SCSIHD_APPLE();
				} else {
					disk[id] = new SCSIHD();
				}
				break;
			case 2:		// MO
				disk[id] = new SCSIMO();
				break;
			case 3:		// CD
				disk[id] = new SCSICD();
				break;
			case 4:		// BRIDGE
				disk[id] = new SCSIBR();
				break;
			default:
				fprintf(fp,	"Error : Invalid device type\n");
				return FALSE;
		}

		// タイプに合わせてコントローラを作る
		if (type == 0) {
			ctrl[id] = new SASIDEV();
		} else {
			ctrl[id] = new SCSIDEV();
		}

		// ドライブはファイルの確認を行う
		if (type <= 1 || (type <= 3 && xstrcasecmp(file, "-") != 0)) {
			// パスを設定
			filepath.SetPath(file);

			// オープン
			if (!disk[id]->Open(filepath)) {
				fprintf(fp, "Error : File open error [%s]\n", file);
				delete disk[id];
				disk[id] = NULL;
				delete ctrl[id];
				ctrl[id] = NULL;
				return FALSE;
			}
		}

		// ライトスルーに設定
		disk[id]->SetCacheWB(FALSE);

		// 新たなディスクを接続
		ctrl[id]->Connect(id, &bus);
		ctrl[id]->SetUnit(0, disk[id]);
		return TRUE;
	}

	// 有効なコマンドか
	if (cmd > 4) {
		fprintf(fp, "Error : Invalid command\n");
		return FALSE;
	}

	// コントローラが存在するか
	if (ctrl[id] == NULL) {
		fprintf(fp, "Error : No such device\n");
		return FALSE;
	}

	// ディスクが存在するか
	if (disk[id] == NULL) {
		fprintf(fp, "Error : No such device\n");
		return FALSE;
	}

	// 切断コマンド
	if (cmd == 1) {					// DETACH
		ctrl[id]->SetUnit(0, NULL);
		delete disk[id];
		disk[id] = NULL;
		delete ctrl[id];
		ctrl[id] = NULL;
		return TRUE;
	}

	// MOかCDの場合だけ有効
	if (disk[id]->GetID() != MAKEID('S', 'C', 'M', 'O') &&
		disk[id]->GetID() != MAKEID('S', 'C', 'C', 'D')) {
		fprintf(fp, "Error : Operation denied(Deveice isn't MO or CD)\n");
		return FALSE;
	}

	switch (cmd) {
		case 2:						// INSERT
			// パスを設定
			filepath.SetPath(file);

			// オープン
			if (!disk[id]->Open(filepath)) {
				fprintf(fp, "Error : File open error [%s]\n", file);
				return FALSE;
			}
			break;

		case 3:						// EJECT
			disk[id]->Eject(TRUE);
			break;

		case 4:						// PROTECT
			if (disk[id]->GetID() != MAKEID('S', 'C', 'M', 'O')) {
				fprintf(fp, "Error : Operation denied(Deveice isn't MO)\n");
				return FALSE;
			}
			disk[id]->WriteP(!disk[id]->IsWriteP());
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
	int i;
	int id;
	int type;
	char *argID;
	char *argPath;
	int len;
	char *ext;

	// IDとパス指定がなければ処理を中断
	if (argc < 3) {
		return TRUE;
	}

	// 引数の解読開始
	i = 1;
	argc--;

	// IDとパスを取得
	while (argc >= 2) {
		argc -= 2;
		argID = argv[i++];
		argPath = argv[i++];

		// -ID or -idの形式をチェック
		if (strlen(argID) != 4 || xstrncasecmp(argID, "-id", 3) != 0) {
			fprintf(stderr,
				"Error : Invalid argument(-IDn) [%s]\n", argID);
			return FALSE;
		}

		// ID番号をチェック(0-7)
		if (argID[3] < '0' || argID[3] > '7') {
			fprintf(stderr,
				"Error : Invalid argument(-IDn n=0-7) [%c]\n", argID[3]);
			return FALSE;
		}

		// ID確定
		id = argID[3] - '0';

		// すでにアクティブなデバイスがあるならスキップ
		if (disk[id]) {
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
				fprintf(stderr,
					"Error : Invalid argument(File path is short) [%s]\n",
					argPath);
				return FALSE;
			}

			// 拡張子を持っているか？
			if (argPath[len - 4] != '.') {
				fprintf(stderr,
					"Error : Invalid argument(No extension) [%s]\n", argPath);
				return FALSE;
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
				fprintf(stderr,
					"Error : Invalid argument(file type) [%s]\n", ext);
				return FALSE;
			}
		}

		// コマンド実行
		if (!ProcessCmd(stderr, id, 0, type, argPath)) {
			return FALSE;
		}
	}

	// デバイスリスト表示
	ListDevice(stdout);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	モニタースレッド
//
//---------------------------------------------------------------------------
static void *MonThread(void *param)
{
	struct sockaddr_in client;
	socklen_t len; 
	int fd;
	FILE *fp;
	BYTE buf[BUFSIZ];
	int id;
	int cmd;
	int type;
	char *file;
	BOOL list;

	// 監視準備
	listen(monfd, 1);

	while (1) {
		// 接続待ち
		memset(&client, 0, sizeof(client)); 
		len = sizeof(client); 
		fd = accept(monfd, (struct sockaddr*)&client, &len);
		if (fd < 0) {
			break;
		}

		// コマンド取得
		fp = fdopen(fd, "r+");
		fgets((char *)buf, BUFSIZ, fp);
		buf[strlen((const char*)buf) - 1] = 0;

		// デバイスリスト表示なのか制御コマンドかを判断
		if (strncasecmp((char*)buf, "list", 4) == 0) {
			list = TRUE;
		} else {
			list = FALSE;
			id = (int)(buf[0] - '0');
			cmd = (int)(buf[2] - '0');
			type = (int)(buf[4] - '0');
			file = (char*)&buf[6];
		}

		// バスフリーのタイミングまで待つ
		while (phase != BUS::busfree) {
			usleep(500 * 1000);
		}

		if (list) {
			// リスト表示
			ListDevice(fp);
		} else {
			// コマンド実行
			ProcessCmd(fp, id, cmd, type, file);
		}

		// 接続解放
		fclose(fp);
		close(fd);
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
//	主処理
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	int i;
	DWORD dwBusBak;
	DWORD dwBusIn;
	struct sched_param schparam;

	// バナー出力
	if (!Banner(argc, argv)) {
		exit(0);
	}

	// 初期化
	if (!Init()) {
		fprintf(stderr, "Error : Initializing\n");

		// 恐らくrootでは無い？
		exit(EPERM);
	}

	// 既に起動している？
	if (!bServer) {
		fprintf(stderr, "Error : Already running RaSCSI\n");
		exit(0);
	}

	// 構築
	if (!ParseArgument(argc, argv)) {
		// クリーンアップ
		Cleanup();

		// 引数エラーで終了
		exit(EINVAL);
	}

	// モニタースレッド生成
	bRun = TRUE;
	pthread_create(&monthread, NULL, MonThread, NULL);

	// リセット
	Reset();

	// メインループ準備
	phase = BUS::busfree;
	dwBusBak = bus.Aquire();

	// メインループ
	while(bRun) {
		// バスの入力信号変化検出
		dwBusIn = bus.Aquire();

		// 入力信号変化検出
		if ((dwBusIn & GPIO_INEDGE) == (dwBusBak & GPIO_INEDGE)) {
			usleep(0);
			continue;
		}

		// バスの入力信号を保存
		dwBusBak = dwBusIn;

		// そもそもセレクション信号が無ければ無視
		if (!bus.GetSEL() || bus.GetBSY()) {
			continue;
		}

		// 全コントローラに通知
		for (i = 0; i < CtrlMax; i++) {
			if (!ctrl[i]) {
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

		// スケジューリングポリシー設定(最優先)
		schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
		sched_setscheduler(0, SCHED_FIFO, &schparam);

		// バスフリーになるまでループ
		while (bRun) {
			// ターゲット駆動
			phase = ctrl[actid]->Process();

			// バスフリーになったら終了
			if (phase == BUS::busfree) {
				break;
			}
		}

		// バスフリーでセッション終了
		actid = -1;
		phase = BUS::busfree;
		bus.Reset();
		dwBusBak = bus.Aquire();

		// スケジューリングポリシー設定(ノーマル)
		schparam.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schparam);
	}

	// クリーンアップ
	Cleanup();

	// 終了
	exit(0);
}
