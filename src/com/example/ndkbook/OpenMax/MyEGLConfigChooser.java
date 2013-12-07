package com.example.ndkbook.OpenMax;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLDisplay;

import android.opengl.GLSurfaceView;

class MyEGLConfigChooser implements GLSurfaceView.EGLConfigChooser {

	private static int EGL_OPENGL_ES2_BIT = 4;
	private static int[] configAttribs = { EGL10.EGL_RED_SIZE, 4,
			EGL10.EGL_GREEN_SIZE, 4, EGL10.EGL_BLUE_SIZE, 4,
			EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL10.EGL_NONE };

	public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {

		int[] num_config = new int[1];
		EGLConfig[] configs = new EGLConfig[1];
		egl.eglChooseConfig(display, configAttribs, configs, 1, num_config);

		int numConfigs = num_config[0];

		if (numConfigs <= 0) {
			throw new IllegalArgumentException("No configs match configSpec");
		}

		return configs[0];
	}
}