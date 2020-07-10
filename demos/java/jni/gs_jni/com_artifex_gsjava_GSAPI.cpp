#include "com_artifex_gsjava_GSAPI.h"

#include <iapi.h>
#include <gdevdsp.h>

#include "jni_util.h"
#include "callbacks.h"

using namespace util;

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1revision
	(JNIEnv *env, jclass, jobject revision, jint len)
{
	if (revision == NULL)
		return throwNullPointerException(env, "Revision object is NULL");
	gsapi_revision_t gsrevision;
	jint code = gsapi_revision(&gsrevision, sizeof(gsapi_revision_t));
	if (code == 0)
	{
		setByteArrayField(env, revision, "product", gsrevision.product);
		setByteArrayField(env, revision, "copyright", gsrevision.copyright);
		setLongField(env, revision, "revision", gsrevision.revision);
		setLongField(env, revision, "revisionDate", gsrevision.revisiondate);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1new_1instance
	(JNIEnv *env, jclass, jobject instance, jlong callerHandle)
{
	if (instance == NULL)
		return throwNullPointerException(env, "LongReference object is NULL");

	void *gsInstance;
	int code = gsapi_new_instance(&gsInstance, (void *)callerHandle);
	if (code == 0)
		setLongField(env, instance, "value", (jlong)gsInstance);
	return code;
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1delete_1instance
	(JNIEnv *, jclass, jlong instance)
{
	gsapi_delete_instance((void *)instance);
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1stdio_1with_1handle
	(JNIEnv *env, jclass, jlong instance, jobject stdIn, jobject stdOut, jobject stdErr, jlong callerHandle)
{
	int code = gsapi_set_stdio_with_handle((void *)instance, callbacks::stdInFunction,
		callbacks::stdOutFunction, callbacks::stdErrFunction, (void *)callerHandle);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setIOCallbacks(stdIn, stdOut, stdErr);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1stdio
	(JNIEnv *env, jclass, jlong instance, jobject stdIn, jobject stdOut, jobject stdErr)
{
	int code = gsapi_set_stdio((void *)instance, callbacks::stdInFunction,
		callbacks::stdOutFunction, callbacks::stdErrFunction);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setIOCallbacks(stdIn, stdOut, stdErr);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1poll_1with_1handle
	(JNIEnv *env, jclass, jlong instance, jobject poll, jlong callerHandle)
{
	int code = gsapi_set_poll_with_handle((void *)instance, callbacks::pollFunction, (void *)callerHandle);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setPollCallback(poll);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1poll
	(JNIEnv *env, jclass, jlong instance, jobject poll)
{
	int code = gsapi_set_poll((void *)instance, callbacks::pollFunction);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setPollCallback(poll);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1display_1callback
	(JNIEnv *env, jclass, jlong instance, jobject displayCallback)
{
	display_callback cb;
	cb.size = sizeof(display_callback);
	cb.version_major = 1;
	cb.version_minor = 0;

	cb.display_open = callbacks::display::displayOpenFunction;
	cb.display_preclose = callbacks::display::displayPrecloseFunction;
	cb.display_close = callbacks::display::displayCloseFunction;
	cb.display_presize = callbacks::display::displayPresizeFunction;
	cb.display_size = callbacks::display::displaySizeFunction;
	cb.display_sync = callbacks::display::displaySyncFunction;
	cb.display_page = callbacks::display::displayPageFunction;
	cb.display_update = callbacks::display::displayUpdateFunction;
	cb.display_memalloc = NULL;
	cb.display_memfree = NULL;
	cb.display_separation = callbacks::display::displaySeparationFunction;
	cb.display_adjust_band_height = callbacks::display::displayAdjustBandHeightFunction;
	cb.display_rectangle_request = callbacks::display::displayRectangleRequestFunction;

	int code = gsapi_set_display_callback((void *)instance, &cb);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setDisplayCallback(displayCallback);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1register_1callout
	(JNIEnv *env, jclass, jlong instance, jobject callout, jlong calloutHandle)
{
	// Only supports registering one callout right now
	int code = gsapi_register_callout((void *)instance, callbacks::calloutFunction, (void *)calloutHandle);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setCalloutCallback(callout);
	}
	return code;
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1deregister_1callout
	(JNIEnv *env, jclass, jlong instance, jobject callout, jlong calloutHandle)
{
	// Only supports deregistering one callout right now
	gsapi_deregister_callout((void *)instance, callbacks::calloutFunction, (void *)calloutHandle);
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1arg_1encoding
	(JNIEnv *env, jclass, jlong instance, jint encoding)
{
	return gsapi_set_arg_encoding((void *)instance, encoding);
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1default_1device_1list
	(JNIEnv *env, jclass, jlong instance, jbyteArray list, jint listlen)
{
	jboolean isCopy = false;
	int code = gsapi_set_default_device_list((void *)instance,
		(const char *)env->GetByteArrayElements(list, &isCopy), listlen);
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1get_1default_1device_1list
	(JNIEnv *env, jclass, jlong instance, jobject list, jobject listlen)
{
	char *clist = NULL;
	int clistlen = 0;
	int code = gsapi_get_default_device_list((void *)instance, &clist, &clistlen);
	if (code == 0)
	{
		ByteArrayReference listRef = ByteArrayReference(env, list);
		listRef.setValue(clist);

		IntReference listlenRef = IntReference(env, listlen);
		listlenRef.setValue(clistlen);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1init_1with_1args
	(JNIEnv *env, jclass, jlong instance, jint argc, jobjectArray argv)
{
	char **cargv = jbyteArray2DToCharArray(env, argv);
	int code = gsapi_init_with_args((void *)instance, argc, cargv);
	delete2DByteArray(argc, cargv);
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1begin
	(JNIEnv *, jclass, jlong, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1continue
	(JNIEnv *, jclass, jlong, jbyteArray, jint, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1end
	(JNIEnv *, jclass, jlong, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1with_1length
	(JNIEnv *, jclass, jlong, jbyteArray, jint, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string
	(JNIEnv *, jclass, jlong, jbyteArray, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1file
	(JNIEnv *, jclass, jlong, jbyteArray, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1exit
	(JNIEnv *, jclass, jlong)
{
	return 0;
}