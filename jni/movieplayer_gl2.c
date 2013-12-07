#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <stdbool.h>

#define  EXTERNAL_STORAGE_PATH "/mnt/sdcard/"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,__func__,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,__func__,__VA_ARGS__)

/* プログラムオブジェクト */
static GLuint gProgram;
/* vPositionハンドル */
static GLuint gvPositionHandle;
/* a_v_texCoordハンドル */
static GLuint gvTexCoordHandle;
/* u_stMatrixハンドル */
static GLuint gvStMatrixHandle;
/* textureハンドル */
static GLuint gvSamplerHandle;

/* テクスチャの幅 */
static const GLuint gtexture_width = 1024;
/* テクスチャの高さ */
static const GLuint gtexture_height = 1024;

/* テクスチャの実際に使われている幅 */
static GLuint gtexture_actual_width;
/* テクスチャの実際に使われている高さ */
static GLuint gtexture_actual_height;

/* スクリーンの幅 */
static GLuint gscreen_width;

/* スクリーンの高さ */
static GLuint gscreen_height;

/* エラーをチェックし、エラーがあった場合にはエラー番号をログへ出力します */
static void checkGlError(const char* op) {
    GLint error;
    for (error = glGetError(); error; error
            = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}

/* バーテックスシェーダコード */
static const char gVertexShader[] = 
    "attribute vec4 vPosition;   \n"
    "varying vec2 v_texCoord;    \n"
    "attribute vec4 a_v_texCoord;\n"
    "uniform mat4 u_stMatrix;\n"
    "void main() {               \n"
    "  gl_Position = vPosition;  \n"
    "  v_texCoord = (u_stMatrix * a_v_texCoord).xy;\n"
    "}                           \n";


/* フラグメントシェーダコード */
static const char gFragmentShader[] = 
 "#extension GL_OES_EGL_image_external : require\n"
 "precision mediump float;     \n"
 "uniform samplerExternalOES texture;\n"
 "varying vec2 v_texCoord;     \n"
 "void main() {                \n"
 "  float p;\n"
 "  p = dot(texture2D(texture, v_texCoord), vec4(0.298, 0.586, 0.114, 0)); \n"
 "  gl_FragColor = vec4(p, p, p, 1);" 
 "}                            \n";


/* シェーダを読み込む */
GLuint loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader;
    
    shader = glCreateShader(shaderType);
    if (shader) {
        /* シェーダのソースコードをセット */
        glShaderSource(shader, 1, &pSource, NULL);
        /* シェーダをコンパイル */
        glCompileShader(shader);
        GLint compiled = 0;
        /* コンパイルの成否を取得 */
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            /* エラーメッセージの長さを取得 */
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    /* エラーメッセージを取得 */
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n",
                            shaderType, buf);
                    free(buf);
                }
                /* シェーダを削除 */
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

/* プログラムオブジェクトを作成する */
GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader;
    GLuint pixelShader;
    GLuint program;
    
    /* バーテックスシェーダをロード */
    vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    /* フラグメントシェーダをロード */
    pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }


    /* 空のプログラムオブジェクトを作成します */
    program = glCreateProgram();
    if (program) {
        /* バーテックスシェーダをプログラムへアタッチ */
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");

        /* フラグメントシェーダをプログラムへアタッチ */
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");

        /* プログラムオブジェクトをリンク */
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;

        /* リンクの成否を取得 */
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;

            /* エラーメッセージの長さを取得 */
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    /* エラーメッセージを取得 */
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }

            /* プログラムオブジェクトを削除 */
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}


bool setupGraphics(int w, int h) {

    gProgram = createProgram(gVertexShader, gFragmentShader);
    if (!gProgram) {
        LOGE("Could not create program.");
        return false;
    }

    /* Vertex ShaderのvPosition変数の位置を取得 */
    gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
    checkGlError("glGetAttribLocation");

    /* Vertex Shaderのa_v_texCoord変数の位置を取得 */
    gvTexCoordHandle = glGetAttribLocation(gProgram, "a_v_texCoord");
    checkGlError("glGetAttribLocation");

    /* Vertex Shaderのu_stMatrix変数の位置を取得 */
    gvStMatrixHandle = glGetUniformLocation(gProgram, "u_stMatrix");
    checkGlError("glGetAttribLocation");

    /* ビューポートを設定 */
    glViewport(0, 0, w, h);
    checkGlError("glViewport");
    
    /* テクスチャのメモリ上のアライメントを設定 */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 
    
    /* スクリーンの幅と高さを保存 */
    gscreen_width = w;
    gscreen_height = h;

    return true;
}

void renderFrame(GLuint textureId, float* stMatrix) {
    const float texture_coord_x = 1.0f;
    const float texture_coord_y = 1.0f;

    GLfloat vVertices[] = {
        -1.0f, -1.0f, 0.0f,                      /* 頂点座標 */
        0.0f, 0.0f,                             /* テクスチャ座標 */

        -1.0, 1.0f, 0.0f,                      /* 頂点座標 */
        0.0f, texture_coord_y,                  /* テクスチャ座標 */

        1.0f, 1.0f, 0.0f,                      /* 頂点座標 */
        texture_coord_x, texture_coord_y,       /* テクスチャ座標 */

        1.0f, -1.0f, 0.0f,                       /* 頂点座標 */
        texture_coord_x, 0.0f                   /* テクスチャ座標 */
    }; 


    GLushort indices[] = { 0, 1, 2, 0, 2, 3 }; 
    GLsizei stride = 5 * sizeof(GLfloat);

    /* カラーバッファをクリアする */
    glClear(GL_COLOR_BUFFER_BIT); 
    checkGlError("glClear"); 

    /* プログラムオブジェクトを導入する */
    glUseProgram(gProgram); 
    checkGlError("glUseProgram"); 


    /* 頂点座標をロード */
    glVertexAttribPointer(gvPositionHandle, 3, GL_FLOAT, GL_FALSE, stride, 
                    vVertices);

    /* テクスチャ座標をロード */
    glVertexAttribPointer(gvTexCoordHandle, 2, GL_FLOAT, GL_FALSE, stride, 
                    &vVertices[3]);

    /* stMatrix座標をロード */
    glUniformMatrix4fv(gvStMatrixHandle, 1, GL_FALSE, stMatrix);

    /* 頂点座標を有効化 */
    glEnableVertexAttribArray(gvPositionHandle);

    /* 頂点座標を有効化 */
    glEnableVertexAttribArray(gvTexCoordHandle); 

    /* テクスチャユニットを設定 */ 
    glActiveTexture(GL_TEXTURE0); 
    /* テクスチャを有効にする */ 
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId); 

    /* 矩形を描画 */
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices); 
} 

JNIEXPORT void JNICALL Java_com_example_ndkbook_OpenMax_MoviePlayerNative_init(JNIEnv * env, jclass clazz,  jint width, jint height)
{
    setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_example_ndkbook_OpenMax_MoviePlayerNative_step(
        JNIEnv * env, jclass clazz, jint textureId, jint width, jint height, jfloatArray stMatrix)
{
    jfloat* p_stMatrix;

    gtexture_actual_width = height;
    gtexture_actual_height = width;

    p_stMatrix = (*env)->GetPrimitiveArrayCritical(env, stMatrix, NULL);
    
    renderFrame(textureId, p_stMatrix);

    (*env)->ReleasePrimitiveArrayCritical(env, stMatrix, p_stMatrix, 0);
}

/* end of file */
