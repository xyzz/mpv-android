#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <locale.h>

extern "C" {
#include <libavutil/jni.h>
}

#include <mpv/client.h>
#include <mpv/opengl_cb.h>

#include <EGL/egl.h>
#include <pthread.h>

#include "main.h"

#define ARRAYLEN(a) (sizeof(a)/sizeof(a[0]))
#define jname(name) Java_is_xyz_mpv_MPVLib_##name
#define jfunc(name, type) JNIEXPORT type JNICALL jname(name)
#define jvoidfunc(name)  jfunc(name, void)

extern "C" {
    jvoidfunc(init) (JNIEnv* env, jobject obj);
    jvoidfunc(destroy) (JNIEnv *env, jobject obj);

    jvoidfunc(command) (JNIEnv* env, jobject obj, jobjectArray jarray);
    jvoidfunc(resize) (JNIEnv* env, jobject obj, jint width, jint height);
    jvoidfunc(step) (JNIEnv* env, jobject obj);
    jvoidfunc(play) (JNIEnv *env, jobject obj);
    jvoidfunc(pause) (JNIEnv *env, jobject obj);
    jvoidfunc(touch_1down) (JNIEnv* env, jobject obj, jint x, jint y);
    jvoidfunc(touch_1move) (JNIEnv* env, jobject obj, jint x, jint y);
    jvoidfunc(touch_1up) (JNIEnv* env, jobject obj, jint x, jint y);
    jvoidfunc(setconfigdir) (JNIEnv* env, jobject obj, jstring path);

    jfunc(getpropertyint, jint) (JNIEnv *env, jobject obj, jstring property);
    jvoidfunc(setpropertyint) (JNIEnv *env, jobject obj, jstring property, jint value);
};

mpv_handle *mpv;
mpv_opengl_cb_context *mpv_gl;

int g_width, g_height;
char g_config_dir[2048];
char **g_command_queue[16] = {NULL};

static void die(const char *msg)
{
    ALOGE("%s", msg);
    exit(1);
}

static void *get_proc_address_mpv(void *fn_ctx, const char *name)
{
    return (void*)eglGetProcAddress(name);
}

static pthread_mutex_t mpv_mutex = PTHREAD_MUTEX_INITIALIZER;

#if 1
#define LOCK() pthread_mutex_lock(&mpv_mutex)
#define UNLOCK() pthread_mutex_unlock(&mpv_mutex)
#else
#define LOCK()
#define UNLOCK()
#endif

jvoidfunc(destroy) (JNIEnv *env, jobject obj) {
    LOCK();
    if (mpv) {
        mpv_opengl_cb_uninit_gl(mpv_gl);
        mpv_gl = NULL;
        mpv_terminate_destroy(mpv);
        mpv = NULL;
    }
    UNLOCK();
}

jvoidfunc(init) (JNIEnv* env, jobject obj) {
    LOCK();
    if (mpv)
        die("init called twice");

    // JavaVM* jvm = NULL;
    // env->GetJavaVM(&jvm);
    // av_jni_register_java_vm(jvm);

    setlocale(LC_NUMERIC, "C");

    mpv = mpv_create();
    if (!mpv)
        die("context init failed");

    int osc = 1;
    mpv_set_option(mpv, "osc", MPV_FORMAT_FLAG, &osc);
    mpv_set_option_string(mpv, "script-opts", "osc-scalewindowed=1.5");

    mpv_set_option_string(mpv, "config", "yes");
    mpv_set_option_string(mpv, "config-dir", g_config_dir);

    mpv_request_log_messages(mpv, "v");

    // mpv_set_option_string(mpv, "vd", "lavc:h264_mediacodec");

    mpv_set_option_string(mpv, "display-fps", "60");
    mpv_set_option_string(mpv, "video-sync", "display-resample");
    // mpv_set_option_string(mpv, "dump-stats", "/sdcard/stats.txt");

    if (mpv_initialize(mpv) < 0)
        die("mpv init failed");

    mpv_gl = (mpv_opengl_cb_context*)mpv_get_sub_api(mpv, MPV_SUB_API_OPENGL_CB);
    if (!mpv_gl)
        die("failed to create mpv GL API handle");

    if (mpv_opengl_cb_init_gl(mpv_gl, NULL, get_proc_address_mpv, NULL) < 0)
        die("failed to initialize mpv GL context");

    if (mpv_set_option_string(mpv, "vo", "opengl-cb") < 0)
        die("failed to set VO");
    if (mpv_set_option_string(mpv, "ao", "opensles:sample-rate=48000:frames-per-buffer=960") < 0)
        die("failed to set AO");

    UNLOCK();
}

static void cq_free(char **e)
{
    for (int i = 0; e[i] != NULL; i++)
        free(e[i]);
    free(e);
}

jvoidfunc(command) (JNIEnv* env, jobject obj, jobjectArray jarray) {
    char **command;
    int jarray_l = env->GetArrayLength(jarray);
    command = (char**) malloc(sizeof(char*) * (jarray_l+1));
    if (!command)
        return;
    for (int i = 0; i < jarray_l; i++) {
        jstring jstr = (jstring) env->GetObjectArrayElement(jarray, i);
        const char *str = env->GetStringUTFChars(jstr, NULL);
        command[i] = strdup(str);
        env->ReleaseStringUTFChars(jstr, str);
    }
    command[jarray_l] = NULL;
    LOCK();
    if (mpv) {
        mpv_command(mpv, (const char**) command);
        cq_free(command);
    }
    UNLOCK();
    // if(cq_push(command) < 0) {
        // ALOGE("command queue full");
        // cq_free(command);
    // }
}

static void mouse_pos(int x, int y) {
    char sx[5], sy[5];
    const char *cmd[] = {"mouse", sx, sy, NULL};
    snprintf(sx, sizeof(sx), "%d", x);
    snprintf(sy, sizeof(sy), "%d", y);
    mpv_command(mpv, cmd);
}

static void mouse_trigger(int down, int btn) {
    // "mouse" doesn't actually send keydown events so we need to do it manually
    char k[16];
    const char *cmd[] = {down?"keydown":"keyup", k, NULL};
    snprintf(k, sizeof(k), "MOUSE_BTN%d", btn);
    mpv_command(mpv, cmd);
}

jvoidfunc(touch_1down) (JNIEnv* env, jobject obj, jint x, jint y) {
    LOCK();
    if (mpv) {
        mouse_pos(x, y);
        mouse_trigger(1, 0);
    }
    UNLOCK();
}

jvoidfunc(touch_1move) (JNIEnv* env, jobject obj, jint x, jint y) {
    LOCK();
    if (mpv)
        mouse_pos(x, y);
    UNLOCK();
}

jvoidfunc(touch_1up) (JNIEnv* env, jobject obj, jint x, jint y) {
    LOCK();
    if (mpv)
        mouse_trigger(0, 0);
    // move the cursor to the top left corner where it doesn't trigger the OSC
    // FIXME: this causes the OSC to receive a mouse_btn0 up event with x and y == 0
    //        but sometimes it gets the correct coords (threading/async?)
    //mouse_pos(0, 0);
    UNLOCK();
}

jvoidfunc(resize) (JNIEnv* env, jobject obj, jint width, jint height) {
    LOCK();
    g_width = width;
    g_height = height;
    UNLOCK();
}

jvoidfunc(step) (JNIEnv* env, jobject obj) {
    LOCK();
    if (mpv) {
        mpv_opengl_cb_report_flip(mpv_gl, 0);
        mpv_opengl_cb_draw(mpv_gl, 0, g_width, -g_height);

        //double avsync = -1;
        //mpv_get_property(mpv, "avsync", MPV_FORMAT_DOUBLE, &avsync);
        //ALOGV("avsync: %.6f\n", avsync);

        while (1) {
            mpv_event *mp_event = mpv_wait_event(mpv, 0);
            mpv_event_log_message *msg;
            if (mp_event->event_id == MPV_EVENT_NONE)
                break;
            switch (mp_event->event_id) {
            case MPV_EVENT_LOG_MESSAGE:
                msg = (mpv_event_log_message*)mp_event->data;
                ALOGV("[%s:%s] %s", msg->prefix, msg->level, msg->text);
                break;
            default:
                ALOGV("event: %s\n", mpv_event_name(mp_event->event_id));
                break;
            }
        }
    }
    UNLOCK();
}

jvoidfunc(play) (JNIEnv* env, jobject obj) {
    LOCK();
    int paused = 0;
    if (mpv)
        mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &paused);
    UNLOCK();
}

jvoidfunc(pause) (JNIEnv* env, jobject obj) {
    LOCK();
    int paused = 1;
    if (mpv)
        mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &paused);
    UNLOCK();
}

jvoidfunc(setconfigdir) (JNIEnv* env, jobject obj, jstring jpath) {
    const char *path = env->GetStringUTFChars(jpath, NULL);
    strncpy(g_config_dir, path, sizeof(g_config_dir) - 1);
    env->ReleaseStringUTFChars(jpath, path);
}

jfunc(getpropertyint, jint) (JNIEnv *env, jobject obj, jstring jproperty) {
    const char *prop = env->GetStringUTFChars(jproperty, NULL);
    int64_t value = 0;
    LOCK();
    if (mpv) {
        int result = mpv_get_property(mpv, prop, MPV_FORMAT_INT64, &value);
        if (result < 0)
            ALOGE("mpv_get_property(%s) returned error %s", prop, mpv_error_string(result));
    }
    UNLOCK();
    env->ReleaseStringUTFChars(jproperty, prop);
    return value;
}

jvoidfunc(setpropertyint) (JNIEnv *env, jobject obj, jstring jproperty, jint value) {
    const char *prop = env->GetStringUTFChars(jproperty, NULL);
    int64_t value64 = value;
    LOCK();
    if (mpv) {
        int result = mpv_set_property(mpv, prop, MPV_FORMAT_INT64, &value64);
        if (result < 0)
            ALOGE("mpv_set_property(%s, %d) returned error %s", prop, value, mpv_error_string(result));
    }
    UNLOCK();
    env->ReleaseStringUTFChars(jproperty, prop);
}
