//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ HDDダンプユーティリティ(イニシーエタモード/SASI Version) ]
//
//	SASI IMAGE EXAMPLE
//		X68000
//			10MB(10441728 BS=256 C=40788)
//			20MB(20748288 BS=256 C=81048)
//			40MB(41496576 BS=256 C=162096)
//
//		MZ-2500/MZ-2800 MZ-1F23
//			20MB(22437888 BS=1024 C=21912)
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "fileio.h"
#include "filepath.h"
#include "gpiobus.h"
#include "rascsi_version.h"

//---------------------------------------------------------------------------
//
// Constant declaration
//
//---------------------------------------------------------------------------
#define BUFSIZE 1024 * 64			// 64KBぐらいかなぁ

//---------------------------------------------------------------------------
//
// Variable declaration
//
//---------------------------------------------------------------------------
GPIOBUS bus;						// バス
int targetid;						// ターゲットデバイスID
int unitid;							// ターゲットユニットID
int bsiz;							// ブロックサイズ
int bnum;							// ブロック数
Filepath hdffile;					// HDFファイル
bool restore;						// リストアフラグ
BYTE buffer[BUFSIZE];				// ワークバッファ
int result;							// 結果コード

//---------------------------------------------------------------------------
//
// Function declaration
//
//---------------------------------------------------------------------------
void Cleanup();

//---------------------------------------------------------------------------
//
// Signal processing
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
	// Stop instruction
	Cleanup();
	exit(0);
}

//---------------------------------------------------------------------------
//
// Banner output
//
//---------------------------------------------------------------------------
bool Banner(int argc, char* argv[])
{
	printf("RaSCSI hard disk dump utility(SASI HDD) ");
	printf("version %s (%s, %s)\n",
			rascsi_get_version_string(),
			__DATE__,
			__TIME__);

	if (argc < 2 || strcmp(argv[1], "-h") == 0) {
		printf("Usage: %s -i ID [-u UT] [-b BSIZE] -c COUNT -f FILE [-r]\n", argv[0]);
		printf(" ID is target device SASI ID {0|1|2|3|4|5|6|7}.\n");
		printf(" UT is target unit ID {0|1}. Default is 0.\n");
		printf(" BSIZE is block size {256|512|1024}. Default is 256.\n");
		printf(" COUNT is block count.\n");
		printf(" FILE is HDF file path.\n");
		printf(" -r is restore operation.\n");
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
//
// Initialization
//
//---------------------------------------------------------------------------
bool Init()
{
	// 割り込みハンドラ設定
	if (signal(SIGINT, KillHandler) == SIG_ERR) {
		return false;
	}
	if (signal(SIGHUP, KillHandler) == SIG_ERR) {
		return false;
	}
	if (signal(SIGTERM, KillHandler) == SIG_ERR) {
		return false;
	}

	// GPIO初期化
	if (!bus.Init(BUS::INITIATOR)) {
		return false;
	}

	// ワーク初期化
	targetid = -1;
	unitid = 0;
	bsiz = 256;
	bnum = -1;
	restore = false;

	return true;
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
// Reset
//
//---------------------------------------------------------------------------
void Reset()
{
	// Reset bus signal line
	bus.Reset();
}

//---------------------------------------------------------------------------
//
//  Argument processing
//
//---------------------------------------------------------------------------
bool ParseArgument(int argc, char* argv[])
{
	int opt;
	char *file;

	// 初期化
	file = NULL;

	// 引数解析
	opterr = 0;
	while ((opt = getopt(argc, argv, "i:u:b:c:f:r")) != -1) {
		switch (opt) {
			case 'i':
				targetid = optarg[0] - '0';
				break;

			case 'u':
				unitid = optarg[0] - '0';
				break;

			case 'b':
				bsiz = atoi(optarg);
				break;

			case 'c':
				bnum = atoi(optarg);
				break;

			case 'f':
				file = optarg;
				break;

			case 'r':
				restore = true;
				break;
		}
	}

	// TARGET IDチェック
	if (targetid < 0 || targetid > 7) {
		fprintf(stderr,
			"Error : Invalid target id range\n");
		return false;
	}

	// UNIT IDチェック
	if (unitid < 0 || unitid > 1) {
		fprintf(stderr,
			"Error : Invalid unit id range\n");
		return false;
	}

	// BSIZチェック
	if (bsiz != 256 && bsiz != 512 && bsiz != 1024) {
		fprintf(stderr,
			"Error : Invalid block size\n");
		return false;
	}

	// BNUMチェック
	if (bnum < 0) {
		fprintf(stderr,
			"Error : Invalid block count\n");
		return false;
	}

	// ファイルチェック
	if (!file) {
		fprintf(stderr,
			"Error : Invalid file path\n");
		return false;
	}

	hdffile.SetPath(file);

	return true;
}

//---------------------------------------------------------------------------
//
// Waiting for phase
//
//---------------------------------------------------------------------------
bool WaitPhase(BUS::phase_t phase)
{
	DWORD now;

	// タイムアウト(3000ms)
	now = SysTimer::GetTimerLow();
	while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000) {
		bus.Aquire();
		if (bus.GetREQ() && bus.GetPhase() == phase) {
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//
//	バスフリーフェーズ実行
//
//---------------------------------------------------------------------------
void BusFree()
{
	// バスリセット
	bus.Reset();
}

//---------------------------------------------------------------------------
//
// Selection phase execution
//
//---------------------------------------------------------------------------
bool Selection(int id)
{
	BYTE data;
	int count;

	// ID設定とSELアサート
	data = 0;
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
// Command phase execution
//
//---------------------------------------------------------------------------
bool Command(BYTE *buf, int length)
{
	int count;

	// フェーズ待ち
	if (!WaitPhase(BUS::command)) {
		return false;
	}

	// コマンド送信
	count = bus.SendHandShake(buf, length);

	// 送信結果が依頼数と同じなら成功
	if (count == length) {
		return true;
	}

	// 送信エラー
	return false;
}

//---------------------------------------------------------------------------
//
// Data in phase execution
//
//---------------------------------------------------------------------------
int DataIn(BYTE *buf, int length)
{
	// フェーズ待ち
	if (!WaitPhase(BUS::datain)) {
		return -1;
	}

	// データ受信
	return bus.ReceiveHandShake(buf, length);
}

//---------------------------------------------------------------------------
//
// Data out phase execution
//
//---------------------------------------------------------------------------
int DataOut(BYTE *buf, int length)
{
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
	cmd[1] = unitid << 5;
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
	count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 6);
	cmd[0] = 0x03;
	cmd[1] = unitid << 5;
	cmd[4] = 4;
	if (!Command(cmd, 6)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	memset(buf, 0x00, 256);
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
//	READ6実行
//
//---------------------------------------------------------------------------
int Read6(int id, DWORD bstart, DWORD blength, DWORD length, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// 結果コード初期化
	result = 0;
	count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 10);
	cmd[0] = 0x8;
	cmd[1] = (BYTE)(bstart >> 16);
	cmd[1] &= 0x1f;
	cmd[1] = unitid << 5;
	cmd[2] = (BYTE)(bstart >> 8);
	cmd[3] = (BYTE)bstart;
	cmd[4] = (BYTE)blength;
	if (!Command(cmd, 6)) {
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
//	WRITE6実行
//
//---------------------------------------------------------------------------
int Write6(int id, DWORD bstart, DWORD blength, DWORD length, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// 結果コード初期化
	result = 0;
	count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 10);
	cmd[0] = 0xa;
	cmd[1] = (BYTE)(bstart >> 16);
	cmd[1] &= 0x1f;
	cmd[1] = unitid << 5;
	cmd[2] = (BYTE)(bstart >> 8);
	cmd[3] = (BYTE)bstart;
	cmd[4] = (BYTE)blength;
	if (!Command(cmd, 6)) {
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
	DWORD duni;
	DWORD dsiz;
	DWORD dnum;
	Fileio fio;
	Fileio::OpenMode omode;
	off_t size;

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
	if (!fio.Open(hdffile.GetPath(), omode)) {
		fprintf(stderr, "Error : Can't open hdf file\n");

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
	printf("UNIT ID                 : %d\n", unitid);

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

	// ブロックサイズとブロック数の表示
	printf("Number of blocks        : %d Blocks\n", bnum);
	printf("Block length            : %d Bytes\n", bsiz);

	// データサイズの表示
	printf("Total length            : %d MBytes %d Bytes\n",
		(bsiz * bnum / 1024 / 1024),
		(bsiz * bnum));

	// リストアファイルサイズの取得
	if (restore) {
		size = fio.GetFileSize();
		printf("Restore file size       : %d bytes", (int)size);
		if (size > (off_t)(bsiz * bnum)) {
			printf("(WARNING : File size is larger than disk size)");
		} else if (size < (off_t)(bsiz * bnum)) {
			printf("(ERROR   : File size is smaller than disk size)\n");
			goto cleanup_exit;
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

	for (i = 0; i < (int)dnum; i++) {
		if (i > 0) {
			printf("\033[21D");
			printf("\033[0K");
		}
		printf("%3d%%(%7d/%7d)",
			(int)((i + 1) * 100 / dnum),
			(int)(i * duni),
			bnum);
		fflush(stdout);

		if (restore) {
			if (fio.Read(buffer, dsiz)) {
				if (Write6(targetid, i * duni, duni, dsiz, buffer) >= 0) {
					continue;
				}
			}
		} else {
			if (Read6(targetid, i * duni, duni, dsiz, buffer) >= 0) {
				if (fio.Write(buffer, dsiz)) {
					continue;
				}
			}
		}

		printf("\n");
		printf("Error occured and aborted... %d\n", result);
		goto cleanup_exit;
	}

	if (dnum > 0) {
		printf("\033[21D");
		printf("\033[0K");
	}

	// 容量上の端数処理
	dnum = bnum % duni;
	dsiz = dnum * bsiz;

	if (dnum > 0) {
		if (restore) {
			if (fio.Read(buffer, dsiz)) {
				Write6(targetid, i * duni, dnum, dsiz, buffer);
			}
		} else {
			if (Read6(targetid, i * duni, dnum, dsiz, buffer) >= 0) {
				fio.Write(buffer, dsiz);
			}
		}
	}

	// 完了メッセージ
	printf("%3d%%(%7d/%7d)\n", 100, bnum, bnum);

cleanup_exit:
	// ファイルクローズ
	fio.Close();

	// クリーンアップ
	Cleanup();

	// 終了
	exit(0);
}
