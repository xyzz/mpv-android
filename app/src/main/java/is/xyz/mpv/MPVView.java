package is.xyz.mpv;

import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

class MPVView extends GLSurfaceView {
    private static final String TAG = "mpv";
    private static final boolean DEBUG = true;

    public MPVView(Context context) {
        super(context);
        init(context);
    }

    public MPVView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public MPVView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs);
        init(context);
    }

    private void init(Context context) {
        // Pick an EGLConfig with RGB8 color, 16-bit depth, no stencil,
        // supporting OpenGL ES 3.0 or later backwards-compatible versions.
        setEGLConfigChooser(8, 8, 8, 0, 16, 0);
        setEGLContextClientVersion(3);
        setPreserveEGLContextOnPause(true);  // TODO: this won't work all the time. we should manually recrete the context in onSurfaceCreated
        setRenderer(new Renderer(this));
    }

    @Override public void onPause() {
        super.onPause();
        MPVLib.pause();
    }

    @Override public void onResume() {
        super.onResume();
        MPVLib.play();
    }

    public void onDestroy() {
        MPVLib.destroy();
    }

    public final Object lock = new Object();
    public String filepath;

    public void loadfile(String filepath) {
        synchronized (lock) {
            this.filepath = filepath;
        }
    }

    @Override public boolean onTouchEvent(MotionEvent ev) {
        final int x = (int) ev.getX(0);
        final int y = (int) ev.getY(0);
        switch(ev.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                MPVLib.touch_down(x, y);
                return true;
            case MotionEvent.ACTION_MOVE:
                MPVLib.touch_move(x, y);
                return true;
            case MotionEvent.ACTION_UP:
                MPVLib.touch_up(x, y);
                return true;
        }
        return super.onTouchEvent(ev);
    }

    private static class Renderer implements GLSurfaceView.Renderer {
        MPVView view;

        public Renderer(MPVView view) {
            this.view = view;
        }

        public void onDrawFrame(GL10 gl) {
            MPVLib.step();
        }

        public void onSurfaceChanged(GL10 gl, int width, int height) {
            MPVLib.resize(width, height);
        }

        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            MPVLib.init();
            MPVLib.command(new String[] { "loadfile", view.filepath });
        }
    }
}
