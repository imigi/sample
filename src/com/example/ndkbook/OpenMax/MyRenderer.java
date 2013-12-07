package com.example.ndkbook.OpenMax;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.graphics.SurfaceTexture;
import android.graphics.SurfaceTexture.OnFrameAvailableListener;
import android.opengl.GLSurfaceView;
import android.os.Environment;

public class MyRenderer implements GLSurfaceView.Renderer,
        OnFrameAvailableListener {

    private MoviePlayer moviePlayer;
//    private final String moviePath = Environment.getExternalStorageDirectory()
//            + "/NativeMedia.ts";
//    private final String moviePath = "/sdcard/Movies/NativeMediaAES.ts";
    private final String moviePath = "/sdcard/Movies/NativeMedia.ts";

    public void onPause() {
        if (moviePlayer != null)
            moviePlayer.pause();
    }

    public void onResume() {
        if (moviePlayer != null)
            moviePlayer.play();
    }

    public void onDestroy() {
        if (moviePlayer != null)
            moviePlayer.destroy();
    }

    //
    public void onDrawFrame(GL10 gl) {
        synchronized (this) {

            moviePlayer.update();
            MoviePlayerNative.step(moviePlayer.getMovieTexture().textureId,
                    1024, 1024, moviePlayer.getMovieTexture().stMatrix);

        }
    }

    public void onSurfaceChanged(GL10 gl, int width, int height) {
        MoviePlayerNative.init(width, height);
    }

    synchronized public void onFrameAvailable(SurfaceTexture surface) {
        synchronized (this) {
            if (moviePlayer.getMovieTexture().surfaceTexture == surface) {
                moviePlayer.getMovieTexture().updateTexture = true;
            }
        }
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        moviePlayer = new MoviePlayer(moviePath, this);
    }
}
