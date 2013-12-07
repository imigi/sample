#include <assert.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <android/log.h>
#define TAG "NativeMedia"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR , TAG, __VA_ARGS__)

#include <OMXAL/OpenMAXAL.h>
#include <OMXAL/OpenMAXAL_Android.h>

#include <android/native_window_jni.h>

// Engineオブジェクト
static XAObjectItf engineObject = NULL;

// Engingインターフェース
static XAEngineItf engineEngine = NULL;

// OutputMixインターフェース
static XAObjectItf outputMixObject = NULL;

// 取得するMediaPlayerのインターフェースの数
#define NUM_MEDIAPLAYER_INTERFACES 4 // XAAndroidBufferQueueItf, XAStreamInformationItf, XAVolumeItf and XAPlayItf

// BufferQueue内のBufferの数
#define NB_BUFFERS 8

// MPEG2-TSのパケットサイズ
#define MPEG2_TS_PACKET_SIZE 188

// バッファ中に含めるMPEG2-TSのパケット数
#define PACKETS_PER_BUFFER 10

// バッファサイズ 
#define BUFFER_SIZE (PACKETS_PER_BUFFER*MPEG2_TS_PACKET_SIZE)

// ストリームの終わりを伝えるための識別子
static const int kEosBufferCntxt = 1980; 

typedef struct
{
    XAObjectItf             playerObj;
    XAPlayItf               playerPlayItf;
    XAAndroidBufferQueueItf playerBQItf;
    XAStreamInformationItf  playerStreamInfoItf;
    XAVolumeItf             playerVolItf;

    char dataCache[BUFFER_SIZE * NB_BUFFERS];
    FILE *file;
    jboolean reachedEof;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    jboolean discontinuity;

    ANativeWindow* theNativeWindow;
    int width;
    int height;

} MoviePlayer;

// ストリーム情報が更新された時に呼ばれます
static void StreamChangeCallback(XAStreamInformationItf caller,
        XAuint32 eventId,
        XAuint32 streamIndex,
        void * pEventData,
        void * pContext )
{
    switch (eventId) {
      case XA_STREAMCBEVENT_PROPERTYCHANGE: {
        XAresult res;
        XAuint32 domain;
        MoviePlayer* player = (MoviePlayer*)pContext;

        res = (*caller)->QueryStreamType(caller, streamIndex, &domain);

        switch (domain) {
          case XA_DOMAINTYPE_VIDEO: {
            XAVideoStreamInformation videoInfo;
            // ストリーム情報を取得する
            res = (*caller)->QueryStreamInformation(caller, streamIndex, &videoInfo);
            assert(XA_RESULT_SUCCESS == res);
            player->width = videoInfo.width;
            player->height = videoInfo.height;
            
          } break;
          default:
            LOGE("Unexpected domain %u\n", domain);
            break;
        }
      } break;
      default:
        LOGE("Unexpected stream event ID %u\n", eventId);
        break;
    }
}

void MoviePlayer_CreateEngine()
{
    XAresult res;

    // エンジンを作成
    res = xaCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == res);

    // エンジンのリソースを確保
    res = (*engineObject)->Realize(engineObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == res);

    // XAEngingItfを取得
    res = (*engineObject)->GetInterface(engineObject, XA_IID_ENGINE, &engineEngine);
    assert(XA_RESULT_SUCCESS == res);

    // OutputMixを作成
    res = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == res);

    // OutputMixのリソースを確保
    res = (*outputMixObject)->Realize(outputMixObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == res);

}

void MoviePlayer_ShutdownEngine()
{
    // OutputMixを破棄します
    if (outputMixObject != NULL) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
    }

    // Engineを破棄します
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }
}


void Java_com_example_ndkbook_OpenMax_MoviePlayer_nativeCreateEngine(JNIEnv* env, jclass clazz)
{
    MoviePlayer_CreateEngine();
}

void Java_com_example_ndkbook_OpenMax_MoviePlayer_nativeShutdownEngine(JNIEnv* env, jclass clazz)
{
    MoviePlayer_ShutdownEngine();
}

bool MoviePlayer_EnqueueInitialBuffers(MoviePlayer* player, bool discontinuity)
{
    size_t bytesRead;
    
    // ファイルから読み込み
    bytesRead = fread(player->dataCache, 1, BUFFER_SIZE * NB_BUFFERS, player->file);

    if (bytesRead <= 0) {
        // ファイル読み込みに失敗した時
        return false;
    }

    if ((bytesRead % MPEG2_TS_PACKET_SIZE) != 0) {
        // 読み込んだサイズがMPEG2-TSのパケットサイズの倍数ではない時
        LOGV("Dropping last packet because it is not whole");
    }

    // 読み込めるパケット数を計算
    size_t packetsRead = bytesRead / MPEG2_TS_PACKET_SIZE;

    size_t i;
    for (i = 0; i < NB_BUFFERS && packetsRead > 0; i++) {
        // 今回のループでコピーするパケット数を計算
        size_t packetsThisBuffer = packetsRead;
        if (packetsThisBuffer > PACKETS_PER_BUFFER) {
            packetsThisBuffer = PACKETS_PER_BUFFER;
        }

        // バッファに入力するサイズを計算する
        size_t bufferSize = packetsThisBuffer * MPEG2_TS_PACKET_SIZE;
        XAresult res;
        if (player->discontinuity) {
            XAAndroidBufferItem items[1];
            items[0].itemKey = XA_ANDROID_ITEMKEY_DISCONTINUITY;
            items[0].itemSize = 0;
            
            // データと
            res = (*player->playerBQItf)->Enqueue(
                    player->playerBQItf,                 // BufferQueueインターフェイス
                    NULL,                                // コールバック時に利用するコンテキスト
                    player->dataCache + i * BUFFER_SIZE, // エンキューするデータ
                    bufferSize,                          // エンキューするデータサイズ
                    items,                               // コールバック時に伝えるアイテム
                    sizeof(XAuint32) * 2                 // コールバック時に伝えるアイテムの長さ 
                    );
            // 不連続に関する処理が終わったので、フラグを下げる
            player->discontinuity = false;
        } else {
            // データをエンキュー
            res = (*player->playerBQItf)->Enqueue(
                    player->playerBQItf,
                    NULL, 
                    player->dataCache + i * BUFFER_SIZE,
                    bufferSize,
                    NULL, 
                    0);
        }
        assert(XA_RESULT_SUCCESS == res);

        // 読み込めるパケット数を減算
        packetsRead -= packetsThisBuffer;
    }

    return true;

}

// MediaPlayerへMPEG2-TSパケットを供給します 
static XAresult AndroidBufferQueueCallback(
        XAAndroidBufferQueueItf caller,
        void *pCallbackContext,        
        void *pBufferContext,          
        void *pBufferData,             
        XAuint32 dataSize,            
        XAuint32 dataUsed,           
        const XAAndroidBufferItem *pItems,
        XAuint32 itemsLength)
{
    XAresult res;
    MoviePlayer* player = pCallbackContext;
    int ok;

    ok = pthread_mutex_lock(&(player->mutex));
    assert(0 == ok);

    // 不連続の通知が着ているか
    if (player->discontinuity) {
        if (!player->reachedEof) {
            // バッファーキューをクリア
            res = (*player->playerBQItf)->Clear(player->playerBQItf);
            assert(XA_RESULT_SUCCESS == res);
            // 読み込み中のファイルの位置を巻き戻す
            rewind(player->file);
            // 一番最初のバッファに不連続であることを付記した上で、バッファリング
            MoviePlayer_EnqueueInitialBuffers(player, true);
        }
        // 不連続に関する処理が終わったのでフラグを下げる
        player->discontinuity = false;
        ok = pthread_cond_signal(&player->cond);
        assert(0 == ok);
        goto exit;
    }

    if ((pBufferData == NULL) && (pBufferContext != NULL)) {
        const int processedCommand = *(int *)pBufferContext;
        if (kEosBufferCntxt == processedCommand) {
            // EOSの時
            LOGV("EOS was processed\n");
            goto exit;
        }
    }

    // EOFに達している時には、そのまま終了
    if (player->reachedEof) {
        goto exit;
    }

    size_t nbRead;
    size_t bytesRead;

    // ファイルからデータを読み込む
    bytesRead = fread(pBufferData, 1, BUFFER_SIZE, player->file);

    if (bytesRead > 0) {
        if ((bytesRead % MPEG2_TS_PACKET_SIZE) != 0) {
            // 読み込んだ長さがMPEG2-TSのパケット長の倍数でない時
            LOGV("Dropping last packet because it is not whole");
        }

        // コピーできるパケット数を計算
        size_t packetsRead = bytesRead / MPEG2_TS_PACKET_SIZE;
        // コピーできるバッファサイズを計算
        size_t bufferSize = packetsRead * MPEG2_TS_PACKET_SIZE;

        // データをエンキュー
        res = (*caller)->Enqueue(caller, NULL,  
                pBufferData,  
                bufferSize,
                NULL ,
                0 );
        assert(XA_RESULT_SUCCESS == res);
    } else {
        // ファイル終端か、エラー時にEOSを送信
        XAAndroidBufferItem msgEos[1];
        msgEos[0].itemKey = XA_ANDROID_ITEMKEY_EOS;
        msgEos[0].itemSize = 0;
        res = (*caller)->Enqueue(caller,
                (void *)&kEosBufferCntxt ,
                NULL,
                0,
                msgEos,
                sizeof(XAuint32) * 2
                );
        assert(XA_RESULT_SUCCESS == res);

        // EOF到達フラグを立てる
        player->reachedEof = true;
    }

exit:
    ok = pthread_mutex_unlock(&player->mutex);
    assert(0 == ok);
    return XA_RESULT_SUCCESS;
}

// MoviePlayerを作成する
MoviePlayer* MoviePlayer_Create(const char* path, ANativeWindow* theNativeWindow)
{
    XAresult res;
    MoviePlayer* player;

    player = (MoviePlayer*)malloc(sizeof(MoviePlayer));
    assert(player);

    memset(player, 0, sizeof(MoviePlayer));

    pthread_mutex_init(&player->mutex, 0 );
    pthread_cond_init(&player->cond, 0 );

    player->file = fopen(path, "rb");
    if (player->file == NULL) {
        return NULL;
    }

    // データの供給源を指定
    XADataLocator_AndroidBufferQueue loc_abq = { XA_DATALOCATOR_ANDROIDBUFFERQUEUE, NB_BUFFERS };
    XADataFormat_MIME format_mime = {
            XA_DATAFORMAT_MIME, XA_ANDROID_MIME_MP2TS, XA_CONTAINERTYPE_MPEG_TS };
    XADataSource dataSrc = {&loc_abq, &format_mime};

    // 音の出力先を設定
    XADataLocator_OutputMix loc_outmix = { XA_DATALOCATOR_OUTPUTMIX, outputMixObject };
    XADataSink audioSnk = { &loc_outmix, NULL };

    // 映像の出力先を設定
    XADataLocator_NativeDisplay loc_nd = {
            XA_DATALOCATOR_NATIVEDISPLAY,
            (void*)theNativeWindow,
            NULL 
    };
    XADataSink imageVideoSink = {&loc_nd, NULL};

    // 利用するインターフェイスとオプションを指定
    XAboolean required[NUM_MEDIAPLAYER_INTERFACES] = {
                    XA_BOOLEAN_TRUE,
                    XA_BOOLEAN_TRUE,
                    XA_BOOLEAN_TRUE,
                    XA_BOOLEAN_TRUE
    };

    XAInterfaceID iidArray[NUM_MEDIAPLAYER_INTERFACES] = {
                    XA_IID_PLAY,
                    XA_IID_ANDROIDBUFFERQUEUESOURCE,
                    XA_IID_STREAMINFORMATION, 
                    XA_IID_VOLUME, 
    };

    // MediaPlayerを作成します
    res = (*engineEngine)->CreateMediaPlayer(
            engineEngine, // エンジンインターフェイス
            &player->playerObj, // 新規に作るプレイヤ
            &dataSrc, // データの入力元
            NULL, // Intrument bank
            &audioSnk, // オーディオデータの出力先
            &imageVideoSink, // 映像データの出力先
            NULL, // Vibra I/O Device用
            NULL, // LED Array用
            NUM_MEDIAPLAYER_INTERFACES, // 要求するインターフェイスの数
            iidArray , // 要求するインターフェイスの種類
            required // 要求するインターフェイスのためのフラグ
            );
    assert(XA_RESULT_SUCCESS == res);


    // MediaPlayerのリソースを確保
    res = (*player->playerObj)->Realize(player->playerObj, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == res);

    // XAPlayItfを取得
    res = (*player->playerObj)->GetInterface(player->playerObj, XA_IID_PLAY, &player->playerPlayItf);
    assert(XA_RESULT_SUCCESS == res);

    // XAStreamInformationItfを取得
    res = (*player->playerObj)->GetInterface(player->playerObj, XA_IID_STREAMINFORMATION, &player->playerStreamInfoItf);
    assert(XA_RESULT_SUCCESS == res);

    // XAVolumeItfを取得
    res = (*player->playerObj)->GetInterface(player->playerObj, XA_IID_VOLUME, &player->playerVolItf);
    assert(XA_RESULT_SUCCESS == res);

    // XAAndroidBufferQueueItfを取得
    res = (*player->playerObj)->GetInterface(player->playerObj, XA_IID_ANDROIDBUFFERQUEUESOURCE, &player->playerBQItf);
    assert(XA_RESULT_SUCCESS == res);

    // どのイベントを通知されるかを指定
    res = (*player->playerBQItf)->SetCallbackEventsMask(player->playerBQItf, XA_ANDROIDBUFFERQUEUEEVENT_PROCESSED);
    assert(XA_RESULT_SUCCESS == res);

    // データを供給するためのコールバック関数を登録します
    res = (*player->playerBQItf)->RegisterCallback(player->playerBQItf, AndroidBufferQueueCallback, player);
    assert(XA_RESULT_SUCCESS == res);

    // ストリーム情報を取得するためのコールバック関数を登録します 
    res = (*player->playerStreamInfoItf)->RegisterStreamChangeCallback(player->playerStreamInfoItf,
            StreamChangeCallback, player);
    assert(XA_RESULT_SUCCESS == res);

    // バッファリングする 
    if (!MoviePlayer_EnqueueInitialBuffers(player, false)) {
        return NULL;
    }

    // 再生準備を行います 
    res = (*player->playerPlayItf)->SetPlayState(player->playerPlayItf, XA_PLAYSTATE_PAUSED);
    assert(XA_RESULT_SUCCESS == res);

    // 最大音量を設定します
    XAmillibel maxLevel;
    (*player->playerVolItf)->GetMaxVolumeLevel(player->playerVolItf, &maxLevel);
    res = (*player->playerVolItf)->SetVolumeLevel(player->playerVolItf, maxLevel);
    assert(XA_RESULT_SUCCESS == res);

    // 再生を開始します
    res = (*player->playerPlayItf)->SetPlayState(player->playerPlayItf, XA_PLAYSTATE_PLAYING);
    assert(XA_RESULT_SUCCESS == res);

    return player;
}

// 再生を開始する
void MoviePlayer_Play(MoviePlayer* player)
{
    XAresult res;
    res = (*player->playerPlayItf)->SetPlayState(player->playerPlayItf, XA_PLAYSTATE_PLAYING);
    assert(XA_RESULT_SUCCESS == res);
}

// 再生を一時停止する
void MoviePlayer_Pause(MoviePlayer* player)
{
    XAresult res;
    res = (*player->playerPlayItf)->SetPlayState(player->playerPlayItf, XA_PLAYSTATE_PAUSED);
    assert(XA_RESULT_SUCCESS == res);
}

// 再生を停止する
void MoviePlayer_Stop(MoviePlayer* player)
{
    XAresult res;
    res = (*player->playerPlayItf)->SetPlayState(player->playerPlayItf, XA_PLAYSTATE_STOPPED);
    assert(XA_RESULT_SUCCESS == res);
}

// MoviePlayerを破棄する
void MoviePlayer_Destroy(MoviePlayer* player)
{
    if(!player)
        return;

    // Playerを破棄する
    if (player->playerObj != NULL) {
        (*player->playerObj)->Destroy(player->playerObj);
        player->playerObj = NULL;
        player->playerPlayItf = NULL;
        player->playerBQItf = NULL;
        player->playerStreamInfoItf = NULL;
        player->playerVolItf = NULL;
    }

    // ファイルを閉じる
    if (player->file != NULL) {
        fclose(player->file);
        player->file = NULL;
    }

    // NativeWindowを解放する
    if (player->theNativeWindow != NULL) {
        ANativeWindow_release(player->theNativeWindow);
        player->theNativeWindow = NULL;
    }

    free(player);

    player = NULL;
}

jint Java_com_example_ndkbook_OpenMax_MoviePlayer_nativeCreate(JNIEnv *env, jobject jobj, jstring jpath, jobject surface)
{
    MoviePlayer* player;
    const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);

    ANativeWindow* theNativeWindow;
    // Native WindowをJava Surfaceから取得
    theNativeWindow = ANativeWindow_fromSurface(env, surface);

    player = MoviePlayer_Create(path, theNativeWindow);

    (*env)->ReleaseStringUTFChars(env, jpath, path);

    return (jint)player;
}

void Java_com_example_ndkbook_OpenMax_MoviePlayer_nativePause(JNIEnv *env, jobject jobj, jint handle)
{
    MoviePlayer* player = (MoviePlayer*)handle;

    MoviePlayer_Pause(player);
}

void Java_com_example_ndkbook_OpenMax_MoviePlayer_nativePlay(JNIEnv *env, jobject jobj, jint handle)
{
    MoviePlayer* player = (MoviePlayer*)handle;

    MoviePlayer_Play(player);
}

void Java_com_example_ndkbook_OpenMax_MoviePlayer_nativeStop(JNIEnv *env, jobject jobj, jint handle)
{
    MoviePlayer* player = (MoviePlayer*)handle;

    MoviePlayer_Stop(player);
}

void Java_com_example_ndkbook_OpenMax_MoviePlayer_nativeDestroy(JNIEnv *env, jobject jobj, jint handle)
{
    MoviePlayer* player = (MoviePlayer*)handle;

    MoviePlayer_Destroy(player);
}
