//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ MFC キュー ]
//
//---------------------------------------------------------------------------

#if !defined(queue_h)
#define queue_h

//===========================================================================
//
//	キュー
//
//===========================================================================
class CQueue
{
public:
	// 内部データ定義
	typedef struct _QUQUEINFO {
		BYTE *pBuf;						// バッファ
		DWORD dwSize;					// サイズ
		DWORD dwMask;					// マスク(サイズ-1)
		DWORD dwRead;					// Readポインタ
		DWORD dwWrite;					// Writeポインタ
		DWORD dwNum;					// 個数
		DWORD dwTotalRead;				// 合計Read
		DWORD dwTotalWrite;				// 合計Write
	} QUEUEINFO, *LPQUEUEINFO;

	// 基本ファンクション
	CQueue();
										// コンストラクタ
	virtual ~CQueue();
										// デストラクタ
	BOOL FASTCALL Init(DWORD dwSize);
										// 初期化

	// API
	void FASTCALL Clear();
										// クリア
	BOOL FASTCALL IsEmpty()	const		{ return (BOOL)(m_Queue.dwNum == 0); }
										// キューが空かチェック
	DWORD FASTCALL GetNum()	const		{ return m_Queue.dwNum; }
										// キューのデータ数を取得
	DWORD FASTCALL Get(BYTE *pDest);
										// キュー内のデータをすべて取得
	DWORD FASTCALL Copy(BYTE *pDest) const;
										// キュー内のデータをすべて取得(キュー進めない)
	void FASTCALL Discard(DWORD dwNum);
										// キューを進める
	void FASTCALL Back(DWORD dwNum);
										// キューを戻す
	DWORD FASTCALL GetFree() const;
										// キューの空き個数を取得
	BOOL FASTCALL Insert(const BYTE *pSrc, DWORD dwLength);
										// キューに挿入
	void FASTCALL GetQueue(QUEUEINFO *pInfo) const;
										// キュー情報取得

private:
	QUEUEINFO m_Queue;
										// 内部ワーク
};

#endif	// queue_h
