/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "ozen.h"

static void *jni_alloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void jni_release(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  free(ptr);
}

static char *copy_bytes(JNIEnv *env, jbyteArray a, size_t *len) {
  if (a == NULL) {
    *len = 0;
    return NULL;
  }
  jsize n = (*env)->GetArrayLength(env, a);
  char *out = malloc((size_t)n + 1);
  if (out != NULL) {
    (*env)->GetByteArrayRegion(env, a, 0, n, (jbyte *)out);
    out[n] = '\0';
  }
  *len = (size_t)n;
  return out;
}

JNIEXPORT jbyteArray JNICALL Java_ozen_Ozen_buildUtf8(JNIEnv *env, jclass cls, jobjectArray paths, jobjectArray texts,
                                                      jbyteArray kitchen, jbyteArray options, jbyteArray entry,
                                                      jint emit) {
  (void)cls;
  jsize n = paths != NULL ? (*env)->GetArrayLength(env, paths) : 0;
  recipe_source *srcs = calloc((size_t)n + 1, sizeof *srcs);
  char **owned = calloc(2 * (size_t)n + 1, sizeof *owned);
  jbyteArray result = NULL;
  if (srcs == NULL || owned == NULL) {
    free(srcs);
    free(owned);
    return NULL;
  }
  for (jsize i = 0; i < n; i++) {
    size_t plen = 0, tlen = 0;
    owned[2 * i] = copy_bytes(env, (jbyteArray)(*env)->GetObjectArrayElement(env, paths, i), &plen);
    owned[2 * i + 1] = copy_bytes(env, (jbyteArray)(*env)->GetObjectArrayElement(env, texts, i), &tlen);
    srcs[i] = (recipe_source){owned[2 * i], owned[2 * i + 1], tlen};
  }
  size_t klen = 0, olen = 0, elen = 0;
  char *k = copy_bytes(env, kitchen, &klen), *o = copy_bytes(env, options, &olen), *e = copy_bytes(env, entry, &elen);
  recipe_config cfg = recipe_default_config();
  cfg.alloc = jni_alloc;
  cfg.release = jni_release;
  cfg.entry = e;
  cfg.emit = (unsigned)emit;
  char *out = recipe_build(&cfg, srcs, (size_t)n, k, klen, o, olen);
  if (out != NULL) {
    jsize len = (jsize)strlen(out);
    result = (*env)->NewByteArray(env, len);
    if (result != NULL) {
      (*env)->SetByteArrayRegion(env, result, 0, len, (const jbyte *)out);
    }
    recipe_free_string(&cfg, out);
  }
  for (jsize i = 0; i < 2 * n; i++) {
    free(owned[i]);
  }
  free(owned);
  free(srcs);
  free(k);
  free(o);
  free(e);
  return result;
}
