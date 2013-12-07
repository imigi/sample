package com.example.ndkbook.OpenMax;

import javax.microedition.khronos.egl.EGL10;

import android.util.Log;

public class MyEGLUtil {
	private static String TAG = "MyEGLUtil";

	public static void checkEglError(String prompt, EGL10 egl) {
		int error;
		while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) {
			Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
		}
	}
}
