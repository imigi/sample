package com.example.ndkbook.OpenMax;

import android.graphics.SurfaceTexture.OnFrameAvailableListener;
import android.view.Surface;

public class MoviePlayer {

    static {
        System.loadLibrary("movieplayer");
    }
	private int handle;
	private MovieTexture movieTexture;

	public MoviePlayer(String path,
			OnFrameAvailableListener onFrameAvailableListener) {
		movieTexture = new MovieTexture(onFrameAvailableListener);
		Surface surface = new Surface(movieTexture.surfaceTexture);
		handle = nativeCreate(path, surface);
		surface.release();
	}

	public static void engineSetup() {
		nativeCreateEngine();
	}

	public static void engineShutdown() {
		nativeShutdownEngine();
	}

	public MovieTexture getMovieTexture() {
		return movieTexture;
	}

	public boolean update() {
		return movieTexture.update();
	}

	public void play() {
		nativePlay(handle);
	}

	public void pause() {
		nativePause(handle);
	}

	public void stop() {
		nativeStop(handle);
	}

	public void destroy() {
		nativeDestroy(handle);
		handle = 0;
	}

	@Override
	protected void finalize() {
		nativeDestroy(handle);
	}

	private static native void nativeCreateEngine();

	private static native void nativeShutdownEngine();

	private native int nativeCreate(String path, Surface surface);

	private native void nativePlay(int handle);

	private native void nativePause(int handle);

	private native void nativeStop(int handle);

	private native void nativeDestroy(int handle);

}
