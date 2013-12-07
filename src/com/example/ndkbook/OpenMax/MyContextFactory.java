package com.example.ndkbook.OpenMax;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;

import android.opengl.GLSurfaceView;

public class MyContextFactory implements GLSurfaceView.EGLContextFactory {
	private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
	private static String TAG = "MyContextFactory";

	public EGLContext createContext(EGL10 egl, EGLDisplay display,
			EGLConfig eglConfig) {

		MyEGLUtil.checkEglError("Before eglCreateContext", egl);
		int[] attrib_list = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE };
		
		// 新しい描画用EGLコンテキストを作成
		EGLContext context = egl.eglCreateContext(display, eglConfig,
				EGL10.EGL_NO_CONTEXT, attrib_list);
		MyEGLUtil.checkEglError("After eglCreateContext", egl);
		
		return context;
	}

	public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) {
		// 描画用EGLコンテキストを破棄
		egl.eglDestroyContext(display, context);
	}
}
 