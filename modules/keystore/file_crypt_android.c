/*****************************************************************************
 * file_crypt_android.c: Crypt using AndroidKeyStore
 *****************************************************************************
 * Copyright © 2016 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>

#include <vlc_common.h>
#include <vlc_memory.h>
#include <vlc_keystore.h>

#include "file_crypt.h"

#include <jni.h>

JNIEnv * android_getEnv(vlc_object_t *, const char *);
#define GET_ENV() android_getEnv(VLC_OBJECT(p_keystore), "android keystore")

static struct
{
    struct
    {
        jmethodID toString;
    } Object;
    struct
    {
        jclass clazz;
        jmethodID getInstance;
        jmethodID load;
        jmethodID getEntry;
        struct
        {
            jmethodID getSecretKey;
        } SecretKeyEntry;
    } KeyStore;
    struct
    {
        jint PURPOSE_ENCRYPT;
        jint PURPOSE_DECRYPT;
        jstring BLOCK_MODE_CBC;
        jstring ENCRYPTION_PADDING_PKCS7;
        jstring KEY_ALGORITHM_AES;
    } KeyProperties;
    struct
    {
        jclass clazz;
        jmethodID getInstance;
        jmethodID init;
        jmethodID generateKey;
    } KeyGenerator;
    struct
    {
        struct
        {
            jclass clazz;
            jmethodID ctor;
            jmethodID setKeySize;
            jmethodID setBlockModes;
            jmethodID setEncryptionPaddings;
            jmethodID build;
        } Builder;
    } KeyGenParameterSpec;
    struct
    {
        jclass clazz;
        jmethodID ctor;
    } IvParameterSpec;
    struct
    {
        jclass clazz;
        jmethodID getInstance;
        jmethodID init;
        jmethodID doFinal;
        jmethodID getIV;
        jint ENCRYPT_MODE;
        jint DECRYPT_MODE;
    } Cipher;
    jstring VLC_CIPHER;
} fields;
static jobject s_jkey = NULL;

#define CALL(caller, obj, method, ...) \
    (*p_env)->caller(p_env, obj, fields.method, ##__VA_ARGS__)

#define CALL_VOID(obj, method, ...) \
    CALL(CallVoidMethod, obj, method, ##__VA_ARGS__)

#define CALL_OBJ(obj, method, ...) \
    CALL(CallObjectMethod, obj, method, ##__VA_ARGS__)

#define CALL_STATICOBJ(obj, method, ...) \
    CALL(CallStaticObjectMethod, fields.obj.clazz, method, ##__VA_ARGS__)

#define CALL_INT(obj, method, ...) \
    CALL(CallIntMethod, obj, method, ##__VA_ARGS__)

#define CALL_BOOL(obj, method, ...) \
    CALL(CallBooleanMethod, obj, method, ##__VA_ARGS__)

#define NEW_OBJECT(arg, ...) \
    (*p_env)->NewObject(p_env, fields.arg.clazz, fields.arg.ctor, ##__VA_ARGS__)

#define NEW_GREF(obj) \
    (*p_env)->NewGlobalRef(p_env, obj)

#define DEL_GREF(obj) \
    (*p_env)->DeleteGlobalRef(p_env, obj)

#define DEL_LREF(obj) \
    (*p_env)->DeleteLocalRef(p_env, obj)

#define NEW_STR(str) \
    (*p_env)->NewStringUTF(p_env, str)

static bool
check_expection(vlc_keystore *p_keystore, JNIEnv *p_env)
{
    jthrowable jex = (*p_env)->ExceptionOccurred(p_env);
    if (jex != NULL)
    {
        (*p_env)->ExceptionClear(p_env);
        if (fields.Object.toString != NULL)
        {
            const char *psz_str = NULL;
            jstring jstr = NULL;

            jstr = (jstring) CALL_OBJ(jex, Object.toString);
            if (jstr != NULL)
                psz_str = (*p_env)->GetStringUTFChars(p_env, jstr, NULL);
            if (psz_str != NULL)
            {
                msg_Err(p_keystore, "%s", psz_str);
                (*p_env)->ReleaseStringUTFChars(p_env, jstr, psz_str);
            }
            DEL_LREF(jstr);
        }
        else
            msg_Err(p_keystore, "unknown exception");
        DEL_LREF(jex);
        return true;
    }
    return false;
}
#define CHECK_EXCEPTION() check_expection(p_keystore, p_env)

#define GET_CLASS(str) do { \
    if (clazz != NULL) \
        DEL_LREF(clazz); \
    clazz = (*p_env)->FindClass(p_env, (str)); \
    if (CHECK_EXCEPTION()) \
        return VLC_EGENERIC; \
} while (0)
#define GET_GLOBAL_CLASS(id) do { \
    fields.id.clazz = (jclass) NEW_GREF(clazz); \
} while (0)
#define GET_ID(get, id, str, args) do { \
    fields.id = (*p_env)->get(p_env, clazz, (str), (args)); \
    if (CHECK_EXCEPTION()) \
        return VLC_EGENERIC; \
} while (0)
#define GET_CONST_INT(id, str) do { \
    jfieldID field = (*p_env)->GetStaticFieldID(p_env, clazz, (str), "I"); \
    if (!CHECK_EXCEPTION()) \
    { \
        fields.id = (*p_env)->GetStaticIntField(p_env, clazz, field); \
        if (CHECK_EXCEPTION()) \
            return VLC_EGENERIC; \
    } else { \
        return VLC_EGENERIC; \
    } \
} while(0)
#define GET_CONST_OBJ(id, str, type) do { \
    jfieldID field = (*p_env)->GetStaticFieldID(p_env, clazz, (str), type); \
    if (!CHECK_EXCEPTION()) \
    { \
        jobject jobj = (*p_env)->GetStaticObjectField(p_env, clazz, field); \
        if (CHECK_EXCEPTION()) \
            return VLC_EGENERIC; \
        fields.id = NEW_GREF(jobj); \
        DEL_LREF(jobj); \
    } else { \
        return VLC_EGENERIC; \
    } \
} while(0)

/*
 * Init JNI fields that will be used to fetch the key and crypt/encrypt
 */
static int
InitJni(vlc_keystore *p_keystore, JNIEnv *p_env)
{
    jclass clazz = NULL;

    GET_CLASS("java/lang/Object");
    GET_ID(GetMethodID, Object.toString, "toString", "()Ljava/lang/String;");

    GET_CLASS("java/security/KeyStore");
    GET_GLOBAL_CLASS(KeyStore);
    GET_ID(GetStaticMethodID, KeyStore.getInstance, "getInstance",
           "(Ljava/lang/String;)Ljava/security/KeyStore;");
    GET_ID(GetMethodID, KeyStore.load, "load",
           "(Ljava/security/KeyStore$LoadStoreParameter;)V");
    GET_ID(GetMethodID, KeyStore.getEntry, "getEntry",
           "(Ljava/lang/String;Ljava/security/KeyStore$ProtectionParameter;)"
           "Ljava/security/KeyStore$Entry;");

    GET_CLASS("java/security/KeyStore$SecretKeyEntry");
    GET_ID(GetMethodID, KeyStore.SecretKeyEntry.getSecretKey, "getSecretKey",
           "()Ljavax/crypto/SecretKey;");


    GET_CLASS("javax/crypto/spec/IvParameterSpec");
    GET_GLOBAL_CLASS(IvParameterSpec);
    GET_ID(GetMethodID, IvParameterSpec.ctor, "<init>", "([B)V");

    GET_CLASS("javax/crypto/Cipher");
    GET_GLOBAL_CLASS(Cipher);
    GET_ID(GetStaticMethodID, Cipher.getInstance, "getInstance",
           "(Ljava/lang/String;)Ljavax/crypto/Cipher;");
    GET_ID(GetMethodID, Cipher.init, "init",
           "(ILjava/security/Key;Ljava/security/spec/AlgorithmParameterSpec;)V");
    GET_ID(GetMethodID, Cipher.doFinal, "doFinal", "([B)[B");
    GET_ID(GetMethodID, Cipher.getIV, "getIV", "()[B");
    GET_CONST_INT(Cipher.ENCRYPT_MODE, "ENCRYPT_MODE");
    GET_CONST_INT(Cipher.DECRYPT_MODE, "DECRYPT_MODE");

    DEL_LREF(clazz);

    jstring VLC_CIPHER = NEW_STR("AES/CBC/PKCS7Padding");
    if (CHECK_EXCEPTION())
        return VLC_EGENERIC;
    fields.VLC_CIPHER = NEW_GREF(VLC_CIPHER);
    DEL_LREF(VLC_CIPHER);

    return VLC_SUCCESS;
}

/*
 * Init JNI fields that will be used by generateKey()
 */
static int
InitJniGenKey(vlc_keystore *p_keystore, JNIEnv *p_env)
{
    jclass clazz = NULL;

    GET_CLASS("android/security/keystore/KeyProperties");
    GET_CONST_INT(KeyProperties.PURPOSE_ENCRYPT, "PURPOSE_ENCRYPT");
    GET_CONST_INT(KeyProperties.PURPOSE_DECRYPT, "PURPOSE_DECRYPT");
    GET_CONST_OBJ(KeyProperties.BLOCK_MODE_CBC,
                  "BLOCK_MODE_CBC", "Ljava/lang/String;");
    GET_CONST_OBJ(KeyProperties.ENCRYPTION_PADDING_PKCS7,
                  "ENCRYPTION_PADDING_PKCS7", "Ljava/lang/String;");
    GET_CONST_OBJ(KeyProperties.KEY_ALGORITHM_AES,
                  "KEY_ALGORITHM_AES", "Ljava/lang/String;");

    GET_CLASS("android/security/keystore/KeyGenParameterSpec$Builder");
    GET_GLOBAL_CLASS(KeyGenParameterSpec.Builder);
    GET_ID(GetMethodID, KeyGenParameterSpec.Builder.ctor, "<init>",
           "(Ljava/lang/String;I)V");
    GET_ID(GetMethodID, KeyGenParameterSpec.Builder.setKeySize, "setKeySize",
           "(I)Landroid/security/keystore/KeyGenParameterSpec$Builder;");
    GET_ID(GetMethodID, KeyGenParameterSpec.Builder.setBlockModes, "setBlockModes",
           "([Ljava/lang/String;)"
           "Landroid/security/keystore/KeyGenParameterSpec$Builder;");
    GET_ID(GetMethodID, KeyGenParameterSpec.Builder.setEncryptionPaddings,
           "setEncryptionPaddings", "([Ljava/lang/String;)"
           "Landroid/security/keystore/KeyGenParameterSpec$Builder;");
    GET_ID(GetMethodID, KeyGenParameterSpec.Builder.build, "build",
           "()Landroid/security/keystore/KeyGenParameterSpec;");

    GET_CLASS("javax/crypto/KeyGenerator");
    GET_GLOBAL_CLASS(KeyGenerator);
    GET_ID(GetStaticMethodID, KeyGenerator.getInstance, "getInstance",
           "(Ljava/lang/String;Ljava/lang/String;)Ljavax/crypto/KeyGenerator;");
    GET_ID(GetMethodID, KeyGenerator.init, "init",
           "(Ljava/security/spec/AlgorithmParameterSpec;)V");
    GET_ID(GetMethodID, KeyGenerator.generateKey, "generateKey",
           "()Ljavax/crypto/SecretKey;");

    DEL_LREF(clazz);
    return VLC_SUCCESS;
}

/*
 * Encrypt or Decrypt
 */
static size_t
Process(vlc_keystore *p_keystore, JNIEnv *p_env, jobject jcipher,
        const uint8_t *p_src, size_t i_src_len,
        const uint8_t *p_iv, uint32_t i_iv_len, uint8_t **pp_dst)
{
    size_t i_dst_size = 0;
    uint8_t *p_dst;
    jbyteArray jsrcArray = NULL, jdstArray = NULL;

    jsrcArray = (*p_env)->NewByteArray(p_env, i_src_len);
    if (CHECK_EXCEPTION())
        goto end;
    (*p_env)->SetByteArrayRegion(p_env, jsrcArray, 0, i_src_len, (jbyte *)p_src);

    jdstArray = (jbyteArray) CALL_OBJ(jcipher, Cipher.doFinal, jsrcArray);
    if (CHECK_EXCEPTION())
        goto end;

    if (jdstArray == NULL)
        goto end;

    jsize dstSize = (*p_env)->GetArrayLength(p_env, jdstArray);

    if (dstSize == 0)
        goto end;

    jbyte *p_bytes = (*p_env)->GetByteArrayElements(p_env, jdstArray, 0);

    p_dst = i_iv_len > 0 ? malloc(dstSize + i_iv_len + sizeof(uint32_t))
                         : malloc(dstSize);
    if (p_dst == NULL)
    {
        (*p_env)->ReleaseByteArrayElements(p_env, jdstArray, p_bytes, 0);
        free(p_dst);
        goto end;
    }

    if (i_iv_len > 0)
    {
        /* Store the IV just before the encrypted password */
        memcpy(p_dst, &i_iv_len, sizeof(uint32_t));
        memcpy(p_dst + sizeof(uint32_t), p_iv, i_iv_len);
        memcpy(p_dst + sizeof(uint32_t) + i_iv_len, p_bytes, dstSize);
        i_dst_size = dstSize + i_iv_len + sizeof(uint32_t);
    }
    else
    {
        memcpy(p_dst, p_bytes, dstSize);
        i_dst_size = dstSize;
    }
    (*p_env)->ReleaseByteArrayElements(p_env, jdstArray, p_bytes, 0);

    *pp_dst = p_dst;

end:
    if (jsrcArray != NULL)
        DEL_LREF(jsrcArray);
    if (jdstArray != NULL)
        DEL_LREF(jdstArray);
    return i_dst_size;
}

static size_t
AndroidEncrypt(vlc_keystore *p_keystore, void *p_ctx, const uint8_t *p_src,
               size_t i_src_len, uint8_t **pp_dst)
{
    (void) p_ctx;
    JNIEnv *p_env = GET_ENV();
    if (p_env == NULL)
        return 0;

    jobject jcipher = NULL;
    jcipher = CALL_STATICOBJ(Cipher, Cipher.getInstance, fields.VLC_CIPHER);
    if (CHECK_EXCEPTION())
        return 0;

    size_t i_dst_len = 0;
    CALL_VOID(jcipher, Cipher.init, fields.Cipher.ENCRYPT_MODE, s_jkey, NULL);
    if (CHECK_EXCEPTION())
        goto end;

    /* Get the IV (Initialization Vector) initialized by Android that will be
     * used to decrypt this secret. This IV will be stored with the encrypted
     * secret */
    jarray jivArray = (jarray) CALL_OBJ(jcipher, Cipher.getIV);
    if (jivArray == NULL)
        goto end;

    jsize i_iv_len = (*p_env)->GetArrayLength(p_env, jivArray);
    if (i_iv_len == 0)
        goto end;
    jbyte *p_iv_bytes = (*p_env)->GetByteArrayElements(p_env, jivArray, 0);

    i_dst_len = Process(p_keystore, p_env, jcipher, p_src, i_src_len,
                        (const uint8_t *)p_iv_bytes, i_iv_len, pp_dst);

    (*p_env)->ReleaseByteArrayElements(p_env, jivArray, p_iv_bytes, 0);
    DEL_LREF(jivArray);

end:

    DEL_LREF(jcipher);
    return i_dst_len;
}

static size_t
AndroidDecrypt(vlc_keystore *p_keystore, void *p_ctx, const uint8_t *p_src,
               size_t i_src_len, uint8_t **pp_dst)
{
    (void) p_ctx;
    JNIEnv *p_env = GET_ENV();
    if (p_env == NULL)
        return 0;

    jobject jivArray = NULL, jiv = NULL, jcipher = NULL;

    jcipher = CALL_STATICOBJ(Cipher, Cipher.getInstance, fields.VLC_CIPHER);
    if (CHECK_EXCEPTION())
        return 0;

    /* Get the IV located at the beginning of the secret */
    size_t i_dst_len = 0;
    uint32_t i_iv_len;
    if (i_src_len < sizeof(uint32_t))
        goto end;

    memcpy(&i_iv_len, p_src, sizeof(uint32_t));
    if (i_iv_len == 0 || i_src_len < (sizeof(uint32_t) + i_iv_len))
        goto end;

    jivArray = (*p_env)->NewByteArray(p_env, i_iv_len);
    if (CHECK_EXCEPTION())
        goto end;
    (*p_env)->SetByteArrayRegion(p_env, jivArray, 0, i_iv_len,
                                 (jbyte *)(p_src + sizeof(uint32_t)) );

    jiv = NEW_OBJECT(IvParameterSpec, jivArray);
    if (CHECK_EXCEPTION())
        goto end;

    /* Use the IV to initialize the decrypt Cipher */
    CALL_VOID(jcipher, Cipher.init, fields.Cipher.DECRYPT_MODE, s_jkey, jiv);
    if (CHECK_EXCEPTION())
        goto end;

    i_dst_len = Process(p_keystore, p_env, jcipher,
                        p_src + sizeof(uint32_t) + i_iv_len,
                        i_src_len - sizeof(uint32_t) - i_iv_len,
                        NULL, 0, pp_dst);

end:
    DEL_LREF(jcipher);
    if (jivArray != NULL)
        DEL_LREF(jivArray);
    if (jiv != NULL)
        DEL_LREF(jiv);
    return i_dst_len;
}

/*
 * Generate a AES/CBC/PKCS7Padding key that will be stored by the Android
 * Keystore
 */
static jobject
GenerateKey(vlc_keystore *p_keystore, JNIEnv *p_env, jstring jstringAlias,
            jstring jstringProvider)
{
    if (InitJniGenKey(p_keystore, p_env) != VLC_SUCCESS)
        return NULL;

    jobject jkey = NULL, jbuilder = NULL, jspec = NULL,
            jkeyGen = NULL;
    jclass jstringClass = NULL;
    jobjectArray jarray = NULL;

    jbuilder = NEW_OBJECT(KeyGenParameterSpec.Builder, jstringAlias,
                          fields.KeyProperties.PURPOSE_ENCRYPT |
                          fields.KeyProperties.PURPOSE_DECRYPT);
    CALL_OBJ(jbuilder, KeyGenParameterSpec.Builder.setKeySize, 256);

    jstringClass = (*p_env)->FindClass(p_env, "java/lang/String");
    if (CHECK_EXCEPTION())
        goto end;

    jarray = (*p_env)->NewObjectArray(p_env, 1, jstringClass, NULL);
    if (CHECK_EXCEPTION())
        goto end;

    (*p_env)->SetObjectArrayElement(p_env, jarray, 0,
                                    fields.KeyProperties.BLOCK_MODE_CBC);
    CALL_OBJ(jbuilder, KeyGenParameterSpec.Builder.setBlockModes, jarray);

    (*p_env)->SetObjectArrayElement(p_env, jarray, 0,
                                    fields.KeyProperties.ENCRYPTION_PADDING_PKCS7);
    CALL_OBJ(jbuilder, KeyGenParameterSpec.Builder.setEncryptionPaddings, jarray);
    jspec = CALL_OBJ(jbuilder, KeyGenParameterSpec.Builder.build);
    if (CHECK_EXCEPTION())
        goto end;

    jkeyGen = CALL_STATICOBJ(KeyGenerator, KeyGenerator.getInstance,
                             fields.KeyProperties.KEY_ALGORITHM_AES,
                             jstringProvider);
    if (CHECK_EXCEPTION())
        goto end;

    CALL_VOID(jkeyGen, KeyGenerator.init, jspec);
    if (CHECK_EXCEPTION())
        goto end;

    jkey = CALL_OBJ(jkeyGen, KeyGenerator.generateKey);
    CHECK_EXCEPTION();

end:
    if (jbuilder != NULL)
        DEL_LREF(jbuilder);
    if (jstringClass != NULL)
        DEL_LREF(jstringClass);
    if (jarray != NULL)
        DEL_LREF(jarray);
    if (jspec != NULL)
        DEL_LREF(jspec);
    if (jkeyGen != NULL)
        DEL_LREF(jkeyGen);

    return jkey;
}

/*
 * Init JNI fields, fetch the key stored by Android or generate a new one
 */
static void
AndroidInit(vlc_keystore *p_keystore)
{
    JNIEnv *p_env = GET_ENV();
    if (p_env == NULL)
        return;

    if (InitJni(p_keystore, p_env) != VLC_SUCCESS)
        return;

    jobject jkeystore = NULL, jentry = NULL, jkey = NULL;
    jstring jstringAlias = NULL, jstringProvider = NULL;

    jstringAlias = NEW_STR("LibVLCAndroid");
    if (CHECK_EXCEPTION())
        goto end;

    jstringProvider = NEW_STR("AndroidKeyStore");
    if (CHECK_EXCEPTION())
        goto end;

    jkeystore = CALL_STATICOBJ(KeyStore, KeyStore.getInstance, jstringProvider);
    if (CHECK_EXCEPTION())
        goto end;

    CALL_VOID(jkeystore, KeyStore.load, NULL);
    if (CHECK_EXCEPTION())
        goto end;

    jentry = CALL_OBJ(jkeystore, KeyStore.getEntry, jstringAlias, NULL);
    if (CHECK_EXCEPTION())
        goto end;
    if (jentry != NULL)
    {
        jkey = CALL_OBJ(jentry, KeyStore.SecretKeyEntry.getSecretKey);
        if (CHECK_EXCEPTION())
            goto end;
    }
    else
    {
        jkey = GenerateKey(p_keystore, p_env, jstringAlias, jstringProvider);
        if (jkey == NULL)
            goto end;
    }

    s_jkey = NEW_GREF(jkey);

end:
    if (jstringAlias != NULL)
        DEL_LREF(jstringAlias);
    if (jstringProvider != NULL)
        DEL_LREF(jstringProvider);
    if (jkeystore != NULL)
        DEL_LREF(jkeystore);
    if (jentry != NULL)
        DEL_LREF(jentry);
    if (jkey != NULL)
        DEL_LREF(jkey);
}

int
CryptInit(vlc_keystore *p_keystore, struct crypt *p_crypt)
{
    static vlc_mutex_t s_lock = VLC_STATIC_MUTEX;
    static bool s_init = false;

    vlc_mutex_lock(&s_lock);
    if (!s_init)
    {
        AndroidInit(p_keystore);
        s_init = true;
    }
    if (s_jkey == NULL)
    {
        vlc_mutex_unlock(&s_lock);
        return VLC_EGENERIC;
    }
    vlc_mutex_unlock(&s_lock);

    p_crypt->pf_encrypt = AndroidEncrypt;
    p_crypt->pf_decrypt = AndroidDecrypt;

    return VLC_SUCCESS;
}
