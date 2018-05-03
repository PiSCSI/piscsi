//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2018 GIMONS
//	[ HDDダンプユーティリティ(イニシーエタモード) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "fileio.h"
#include "filepath.h"
#include "gpiobus.h"

//---------------------------------------------------------------------------
//
//	定数宣言
//
//---------------------------------------------------------------------------
#define BUFSIZE 1024 * 64			// 64KBぐらいかなぁ

//---------------------------------------------------------------------------
//
//	変数宣言
//
//---------------------------------------------------------------------------
GPIOBUS bus;						// バス
int targetid;						// ターゲットデバイスID
int boardid;						// ボードID(自身のID)
Filepath hdsfile;					// HDSファイル
BOOL restore;						// リストアフラグ
BYTE buffer[BUFSIZE];				// ワークバッファ
int result;							// 結果コード

//---------------------------------------------------------------------------
//
//	関数宣言
//
//---------------------------------------------------------------------------
void Cleanup();

//---------------------------------------------------------------------------
//
//	シグナル処理
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
	// 停止指示
	Cleanup();
	exit(0);
}

//---------------------------------------------------------------------------
//
//	バナー出力
//
//---------------------------------------------------------------------------
BOOL Banner(int argc, char* argv[])
{
	printf("RaSCSI hard disk dump utility ");
	printf("version %01d.%01d%01d\n",
		(int)((VERSION >> 8) & 0xf),
		(int)((VERSION >> 4) & 0xf),
		(int)((VERSION     ) & 0xf));

	if (argc < 2 || strcmp(argv[1], "-h") == 0) {
		printf("Usage: %s -i ID [-b BID] -f FILE [-r]\n", argv[0]);
		printf(" ID is target device SCSI ID {0|1|2|3|4|5|6|7}.\n");
		printf(" BID is rascsi board SCSI ID {0|1|2|3|4|5|6|7}. Default is 7.\n");
		printf(" FILE is HDS file path.\n");
		printf(" -r is restore operation.\n");
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
	if (!bus.Init(BUS::INITIATOR)) {
		return FALSE;
	}

	// ワーク初期化
	targetid = -1;
	boardid = 7;
	restore = FALSE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	クリーンアップ
//
//---------------------------------------------------------------------------
void Cleanup()
{
	// バスをクリーンアップ
	bus.Cleanup();
}

//---------------------------------------------------------------------------
//
//	リセット
//
//---------------------------------------------------------------------------
void Reset()
{
	// バス信号線をリセット
	bus.Reset();
}

//---------------------------------------------------------------------------
//
//	引数処理
//
//---------------------------------------------------------------------------
BOOL ParseArgument(int argc, char* argv[])
{
	int opt;
	char *file;

	// 初期化
	file = NULL;

	// 引数解析
	opterr = 0;
	while ((opt = getopt(argc, argv, "i:b:f:r")) != -1) {
		switch (opt) {
			case 'i':
				targetid = optarg[0] - '0';
				break;

			case 'b':
				boardid = optarg[0] - '0';
				break;

			case 'f':
				file = optarg;
				break;

			case 'r':
				restore = TRUE;
				break;
		}
	}

	// TARGET IDチェック
	if (targetid < 0 || targetid > 7) {
		fprintf(stderr,
			"Error : Invalid target id range\n");
		return FALSE;
	}

	// BOARD IDチェック
	if (boardid < 0 || boardid > 7) {
		fprintf(stderr,
			"Error : Invalid board id range\n");
		return FALSE;
	}

	// TARGETとBOARDのID重複チェック
	if (targetid == boardid) {
		fprintf(stderr,
			"Error : Invalid target or board id\n");
		return FALSE;
	}

	// ファイルチェック
	if (!file) {
		fprintf(stderr,
			"Error : Invalid file path\n");
		return FALSE;
	}

	hdsfile.SetPath(file);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	フェーズ待ち
//
//---------------------------------------------------------------------------
BOOL WaitPhase(BUS::phase_t phase)
{
	int count;

	// REQを待つ(6秒)
	count = 30000;
	do {
		usleep(200);
		bus.Aquire();
		if (bus.GetREQ()) {
			break;
		}
	} while (count--);

	// フェーズが一致すればOK
	bus.Aquire();
	if (bus.GetPhase() == phase) {
		return TRUE;
	}

	// フェーズが一致すればOK(リトライ)
	usleep(1000 * 1000);
	bus.Aquire();
	if (bus.GetPhase() == phase) {
		return TRUE;
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	バスフリーフェーズ実行
//
//---------------------------------------------------------------------------
void BusFree()
{
	bus.Reset();
}

//---------------------------------------------------------------------------
//
//	セレクションフェーズ実行
//
//---------------------------------------------------------------------------
BOOL Selection(int id)
{
	BYTE data;
	int count;

	// ID設定とSELアサート
	data = 0;
	data |= (1 << boardid);
	data |= (1 << id);
	bus.SetDAT(data);
	bus.SetSEL(TRUE);

	// BSYを待つ
	count = 10000;
	do {
		usleep(20);
		bus.Aquire();
		if (bus.GetBSY()) {
			break;
		}
	} while (count--);

	// SELネゲート
	bus.SetSEL(FALSE);

	// ターゲットがビジー状態なら成功
	return bus.GetBSY();
}

//---------------------------------------------------------------------------
//
//	コマンドフェーズ実行
//
//---------------------------------------------------------------------------
BOOL Command(BYTE *buf, int length)
{
	int count;

	// フェーズ待ち
	if (!WaitPhase(BUS::command)) {
		return FALSE;
	}

	// コマンド送信
	count = bus.SendHandShake(buf, length);

	// 送信結果が依頼数と同じなら成功
	if (count == length) {
		return TRUE;
	}

	// 送信エラー
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	データインフェーズ実行
//
//---------------------------------------------------------------------------
int DataIn(BYTE *buf, int length)
{
	int count;

	// フェーズ待ち
	if (!WaitPhase(BUS::datain)) {
		return -1;
	}

	// データ受信
	return bus.ReceiveHandShake(buf, length);
}

//---------------------------------------------------------------------------
//
//	データアウトフェーズ実行
//
//---------------------------------------------------------------------------
int DataOut(BYTE *buf, int length)
{
	int count;

	// フェーズ待ち
	if (!WaitPhase(BUS::dataout)) {
		return -1;
	}

	// データ受信
	return bus.SendHandShake(buf, length);
}

//---------------------------------------------------------------------------
//
//	ステータスフェーズ実行
//
//---------------------------------------------------------------------------
int Status()
{
	BYTE buf[256];

	// フェーズ待ち
	if (!WaitPhase(BUS::status)) {
		return -2;
	}

	// データ受信
	if (bus.ReceiveHandShake(buf, 1) == 1) {
		return (int)buf[0];
	}

	// 受信エラー
	return -1;
}

//---------------------------------------------------------------------------
//
//	メッセージインフェーズ実行
//
//---------------------------------------------------------------------------
int MessageIn()
{
	BYTE buf[256];

	// フェーズ待ち
	if (!WaitPhase(BUS::msgin)) {
		return -2;
	}

	// データ受信
	if (bus.ReceiveHandShake(buf, 1) == 1) {
		return (int)buf[0];
	}

	// 受信エラー
	return -1;
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY実行
//
//---------------------------------------------------------------------------
int TestUnitReady(int id)
{
	BYTE cmd[256];

	// 結果コード初期化
	result = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 6);
	cmd[0] = 0x00;
	if (!Command(cmd, 6)) {
		result = -2;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// バスフリー
	BusFree();

	return result;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE実行
//
//---------------------------------------------------------------------------
int RequestSense(int id, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// 結果コード初期化
	result = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 6);
	cmd[0] = 0x03;
	cmd[4] = 0xff;
	if (!Command(cmd, 6)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	memset(buf, 0x00, sizeof(buf));
	count = DataIn(buf, 256);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// バスフリー
	BusFree();

	// 成功であれば転送数を返す
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	MODE SENSE実行
//
//---------------------------------------------------------------------------
int ModeSense(int id, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// 結果コード初期化
	result = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 6);
	cmd[0] = 0x1a;
	cmd[2] = 0x3f;
	cmd[4] = 0xff;
	if (!Command(cmd, 6)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	memset(buf, 0x00, sizeof(buf));
	count = DataIn(buf, 256);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// バスフリー
	BusFree();

	// 成功であれば転送数を返す
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	INQUIRY実行
//
//---------------------------------------------------------------------------
int Inquiry(int id, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// 結果コード初期化
	result = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 6);
	cmd[0] = 0x12;
	cmd[4] = 0xff;
	if (!Command(cmd, 6)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	memset(buf, 0x00, sizeof(buf));
	count = DataIn(buf, 255);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// バスフリー
	BusFree();

	// 成功であれば転送数を返す
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	READ CAPACITY実行
//
//---------------------------------------------------------------------------
int ReadCapacity(int id, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// 結果コード初期化
	result = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 10);
	cmd[0] = 0x25;
	if (!Command(cmd, 10)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	memset(buf, 0x00, sizeof(buf));
	count = DataIn(buf, 8);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// バスフリー
	BusFree();

	// 成功であれば転送数を返す
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	READ10実行
//
//---------------------------------------------------------------------------
int Read10(int id, DWORD bstart, DWORD blength, DWORD length, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// 結果コード初期化
	result = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 10);
	cmd[0] = 0x28;
	cmd[2] = (BYTE)(bstart >> 24);
	cmd[3] = (BYTE)(bstart >> 16);
	cmd[4] = (BYTE)(bstart >> 8);
	cmd[5] = (BYTE)bstart;
	cmd[7] = (BYTE)(blength >> 8);
	cmd[8] = (BYTE)blength;
	if (!Command(cmd, 10)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	count = DataIn(buf, length);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// バスフリー
	BusFree();

	// 成功であれば転送数を返す
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	WRITE10実行
//
//---------------------------------------------------------------------------
int Write10(int id, DWORD bstart, DWORD blength, DWORD length, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// 結果コード初期化
	result = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 10);
	cmd[0] = 0x2a;
	cmd[2] = (BYTE)(bstart >> 24);
	cmd[3] = (BYTE)(bstart >> 16);
	cmd[4] = (BYTE)(bstart >> 8);
	cmd[5] = (BYTE)bstart;
	cmd[7] = (BYTE)(blength >> 8);
	cmd[8] = (BYTE)blength;
	if (!Command(cmd, 10)) {
		result = -2;
		goto exit;
	}

	// DATAOUT
	count = DataOut(buf, length);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// バスフリー
	BusFree();

	// 成功であれば転送数を返す
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	主処理
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	int i;
	int count;
	char str[32];
	DWORD bsiz;
	DWORD bnum;
	DWORD duni;
	DWORD dsiz;
	DWORD dnum;
	DWORD rest;
	Fileio fio;
	Fileio::OpenMode omode;
	off64_t size;

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

	// 構築
	if (!ParseArgument(argc, argv)) {
		// クリーンアップ
		Cleanup();

		// 引数エラーで終了
		exit(EINVAL);
	}

	// リセット
	Reset();

	// ファイルオープン
	if (restore) {
		omode = Fileio::ReadOnly;
	} else {
		omode = Fileio::WriteOnly;
	}
	if (!fio.Open(hdsfile.GetPath(), omode)) {
		fprintf(stderr, "Error : Can't open hds file\n");

		// クリーンアップ
		Cleanup();
		exit(EPERM);
	}

	// バスフリー
	BusFree();

	// RESETシグナル発行
	bus.SetRST(TRUE);
	usleep(1000);
	bus.SetRST(FALSE);

	// ダンプ開始
	printf("TARGET ID               : %d\n", targetid);
	printf("BORAD ID                : %d\n", boardid);

	// TEST UNIT READY
	count = TestUnitReady(targetid);
	if (count < 0) {
		fprintf(stderr, "TEST UNIT READY ERROR %d\n", count);
		goto cleanup_exit;
	}

	// REQUEST SENSE(for CHECK CONDITION)
	count = RequestSense(targetid, buffer);
	if (count < 0) {
		fprintf(stderr, "REQUEST SENSE ERROR %d\n", count);
		goto cleanup_exit;
	}

	// INQUIRY
	count = Inquiry(targetid, buffer);
	if (count < 0) {
		fprintf(stderr, "INQUIRY ERROR %d\n", count);
		goto cleanup_exit;
	}

	// INQUIRYの情報を表示
	memset(str, 0x00, sizeof(str));
	memcpy(str, &buffer[8], 8);
	printf("Vendor                  : %s\n", str);
	memset(str, 0x00, sizeof(str));
	memcpy(str, &buffer[16], 16);
	printf("Product                 : %s\n", str);
	memset(str, 0x00, sizeof(str));
	memcpy(str, &buffer[32], 4);
	printf("Revison                 : %s\n", str);

	// 容量取得
	count = ReadCapacity(targetid, buffer);
	if (count < 0) {
		fprintf(stderr, "READ CAPACITY ERROR %d\n", count);
		goto cleanup_exit;
	}

	// ブロックサイズとブロック数の表示
	bsiz =
		(buffer[4] << 24) | (buffer[5] << 16) |
		(buffer[6] << 8) | buffer[7];
	bnum =
		(buffer[0] << 24) | (buffer[1] << 16) |
		(buffer[2] << 8) | buffer[3];
	bnum++;
	printf("Number of blocks        : %d Blocks\n", bnum);
	printf("Block length            : %d Bytes\n", bsiz);
	printf("Unit Capacity           : %d MBytes %d Bytes\n",
		bsiz * bnum / 1024 / 1024,
		bsiz * bnum);

	// リストアファイルサイズの取得
	if (restore) {
		size = fio.GetFileSize();
		printf("Restore file size       : %d bytes", (int)size);
		if (size != (off64_t)(bsiz * bnum)) {
			printf("(WARNING : File size isn't equal to disk size)");
		}
		printf("\n");
	}

	// バッファサイズ毎にダンプする
	duni = BUFSIZE;
	duni /= bsiz;
	dsiz = BUFSIZE;
	dnum = bnum * bsiz;
	dnum /= BUFSIZE;

	if (restore) {
		printf("Restore progress        : ");
	} else {
		printf("Dump progress           : ");
	}

	for (i = 0; i < dnum; i++) {
		if (i > 0) {
			printf("\033[21D");
			printf("\033[0K");
		}
		printf("%3d\%(%7d/%7d)", (i + 1) * 100 / dnum, i * duni, bnum);
		fflush(stdout);

		if (restore) {
			if (fio.Read(buffer, dsiz)) {
				if (Write10(targetid, i * duni, duni, dsiz, buffer) >= 0) {
					continue;
				}
			}
		} else {
			if (Read10(targetid, i * duni, duni, dsiz, buffer) >= 0) {
				if (fio.Write(buffer, dsiz)) {
					continue;
				}
			}
		}

		printf("\n");
		printf("Error occured and aborted... %d\n", result);
		goto cleanup_exit;
	}

	// 容量上の端数処理
	dnum = bnum % duni;
	dsiz = dnum * bsiz;
	if (dnum > 0) {
		if (restore) {
			if (fio.Read(buffer, dsiz)) {
				Write10(targetid, i * duni, duni, dsiz, buffer);
			}
		} else {
			if (Read10(targetid, i * duni, dnum, dsiz, buffer) >= 0) {
				fio.Write(buffer, dsiz);
			}
		}
	}

	// 完了メッセージ
	printf("\033[21D");
	printf("\033[0K");
	printf("%3d\%(%7d/%7d)\n", 100, bnum, bnum);

cleanup_exit:
	// ファイルクローズ
	fio.Close();

	// クリーンアップ
	Cleanup();

	// 終了
	exit(0);
}
