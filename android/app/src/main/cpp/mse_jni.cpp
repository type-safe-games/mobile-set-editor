// Mobile Set Editor - the JNI layer over the facade (ADR 0006, ADR 0009)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The functions of org.mobileseteditor.core.NativeCore. Each is a thin call into the facade
// (facade/facade.hpp), which runs on the one core thread the Kotlin side owns (Core.kt) and
// returns its queued messages. Strings cross as real UTF-8 (ADR 0002), through String(byte[],
// UTF-8) and String.getBytes(UTF-8) rather than JNI's modified UTF-8. No C++ exception crosses
// JNI: anything that escapes becomes a Failed result.
//
// This is the subset M3 needs; the rest of the facade comes with M4.

#include <util/prec.hpp>
#include <facade/facade.hpp>
#include <gfx/canvas.hpp>
#include <gfx/image_codec.hpp>
#include <gfx/skia/skia_canvas.hpp>
#include <gfx/skia/skia_image_codec.hpp>

#include <android/bitmap.h>
#include <jni.h>

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>

namespace F = mse::facade;

namespace {

// ----------------------------------------------------------------------------- : Classes

constexpr const char* PKG = "org/mobileseteditor/core/";

struct Classes {
  jclass string, message, result, open_result, card_list_result, card_size_result, field_region, render_result;
  jclass scanned_package, scan_result, imported_package, import_result, template_info, template_list_result, progress_listener;
  jmethodID string_from_bytes, string_get_bytes;
  jmethodID message_init, result_init, open_result_init, card_list_result_init, card_size_result_init,
            field_region_init, render_result_init;
  jmethodID scanned_package_init, scan_result_init, imported_package_init, import_result_init,
            template_info_init, template_list_result_init, progress_on_progress;
  jstring utf8;
} cls;

jclass global_class(JNIEnv* env, const std::string& name) {
  jclass local = env->FindClass(name.c_str());
  jclass global = (jclass)env->NewGlobalRef(local);
  env->DeleteLocalRef(local);
  return global;
}

bool load_classes(JNIEnv* env) {
  std::string p = PKG;
  cls.string           = global_class(env, "java/lang/String");
  cls.message          = global_class(env, p + "CoreMessage");
  cls.result           = global_class(env, p + "CoreResult");
  cls.open_result      = global_class(env, p + "OpenResult");
  cls.card_list_result = global_class(env, p + "CardListResult");
  cls.card_size_result = global_class(env, p + "CardSizeResult");
  cls.field_region     = global_class(env, p + "FieldRegion");
  cls.render_result    = global_class(env, p + "RenderResult");
  cls.scanned_package      = global_class(env, p + "ScannedPackage");
  cls.scan_result          = global_class(env, p + "ScanResult");
  cls.imported_package     = global_class(env, p + "ImportedPackage");
  cls.import_result        = global_class(env, p + "ImportResult");
  cls.template_info        = global_class(env, p + "TemplateInfo");
  cls.template_list_result = global_class(env, p + "TemplateListResult");
  cls.progress_listener    = global_class(env, p + "ImportProgressListener");
  if (env->ExceptionCheck()) return false;
  std::string result_sig = "L" + p + "CoreResult;";
  cls.string_from_bytes     = env->GetMethodID(cls.string, "<init>", "([BLjava/lang/String;)V");
  cls.string_get_bytes      = env->GetMethodID(cls.string, "getBytes", "(Ljava/lang/String;)[B");
  cls.message_init          = env->GetMethodID(cls.message, "<init>", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  cls.result_init           = env->GetMethodID(cls.result, "<init>", ("(ILjava/lang/String;Ljava/lang/String;[L" + p + "CoreMessage;)V").c_str());
  cls.open_result_init      = env->GetMethodID(cls.open_result, "<init>", ("(" + result_sig + "J)V").c_str());
  cls.card_list_result_init = env->GetMethodID(cls.card_list_result, "<init>", ("(" + result_sig + "[Ljava/lang/String;[Ljava/lang/String;)V").c_str());
  cls.card_size_result_init = env->GetMethodID(cls.card_size_result, "<init>", ("(" + result_sig + "DD)V").c_str());
  cls.field_region_init     = env->GetMethodID(cls.field_region, "<init>", "(Ljava/lang/String;DDDDDIZ)V");
  cls.render_result_init    = env->GetMethodID(cls.render_result, "<init>", ("(" + result_sig + "[L" + p + "FieldRegion;)V").c_str());
  cls.scanned_package_init  = env->GetMethodID(cls.scanned_package, "<init>",
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JILjava/lang/String;)V");
  cls.scan_result_init      = env->GetMethodID(cls.scan_result, "<init>", ("(" + result_sig + "[L" + p + "ScannedPackage;IJIJII)V").c_str());
  cls.imported_package_init = env->GetMethodID(cls.imported_package, "<init>", "(Ljava/lang/String;Ljava/lang/String;IZLjava/lang/String;)V");
  cls.import_result_init    = env->GetMethodID(cls.import_result, "<init>", ("(" + result_sig + "[L" + p + "ImportedPackage;IIZ)V").c_str());
  cls.template_info_init    = env->GetMethodID(cls.template_info, "<init>",
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  cls.template_list_result_init = env->GetMethodID(cls.template_list_result, "<init>", ("(" + result_sig + "[L" + p + "TemplateInfo;)V").c_str());
  cls.progress_on_progress  = env->GetMethodID(cls.progress_listener, "onProgress", "(JJ)Z");
  jstring utf8 = env->NewStringUTF("UTF-8");
  cls.utf8 = (jstring)env->NewGlobalRef(utf8);
  env->DeleteLocalRef(utf8);
  return !env->ExceptionCheck();
}

// ----------------------------------------------------------------------------- : Strings

jstring to_java(JNIEnv* env, const std::string& s) {
  jbyteArray bytes = env->NewByteArray((jsize)s.size());
  env->SetByteArrayRegion(bytes, 0, (jsize)s.size(), (const jbyte*)s.data());
  jstring out = (jstring)env->NewObject(cls.string, cls.string_from_bytes, bytes, cls.utf8);
  env->DeleteLocalRef(bytes);
  return out;
}

std::string from_java(JNIEnv* env, jstring s) {
  if (!s) return std::string();
  jbyteArray bytes = (jbyteArray)env->CallObjectMethod(s, cls.string_get_bytes, cls.utf8);
  jsize n = env->GetArrayLength(bytes);
  std::string out((size_t)n, '\0');
  env->GetByteArrayRegion(bytes, 0, n, (jbyte*)out.data());
  env->DeleteLocalRef(bytes);
  return out;
}

jobjectArray to_java(JNIEnv* env, const std::vector<std::string>& v) {
  jobjectArray out = env->NewObjectArray((jsize)v.size(), cls.string, nullptr);
  for (size_t i = 0 ; i < v.size() ; ++i) {
    jstring s = to_java(env, v[i]);
    env->SetObjectArrayElement(out, (jsize)i, s);
    env->DeleteLocalRef(s);
  }
  return out;
}

// ----------------------------------------------------------------------------- : Results

jobject to_java(JNIEnv* env, const F::Result& r) {
  jobjectArray messages = env->NewObjectArray((jsize)r.messages.size(), cls.message, nullptr);
  for (size_t i = 0 ; i < r.messages.size() ; ++i) {
    const F::Message& m = r.messages[i];
    jstring text = to_java(env, m.text), card = to_java(env, m.card_uid), field = to_java(env, m.field);
    jobject jm = env->NewObject(cls.message, cls.message_init, (jint)m.severity, text, card, field);
    env->SetObjectArrayElement(messages, (jsize)i, jm);
    env->DeleteLocalRef(jm);
    env->DeleteLocalRef(text);
    env->DeleteLocalRef(card);
    env->DeleteLocalRef(field);
  }
  jstring error = to_java(env, r.error), missing = to_java(env, r.missing_package);
  jobject out = env->NewObject(cls.result, cls.result_init, (jint)r.status, error, missing, messages);
  env->DeleteLocalRef(error);
  env->DeleteLocalRef(missing);
  env->DeleteLocalRef(messages);
  return out;
}

/// A failed result for something that went wrong in this layer
template <typename R>
R failed(const std::string& why, F::Status status = F::Status::Failed) {
  R r;
  r.status = status;
  r.error = why;
  return r;
}

/// Run a call, turning any escaping exception into a failed result
template <typename R, typename Fun>
R guarded(Fun fun) {
  try {
    return fun();
  } catch (const std::exception& e) {
    return failed<R>(std::string("internal error: ") + e.what());
  } catch (...) {
    return failed<R>("internal error");
  }
}

// ----------------------------------------------------------------------------- : Opening from a file descriptor

/// Copy everything readable from fd into a file
bool copy_fd_to_file(int fd, const std::string& path) {
  FILE* out = std::fopen(path.c_str(), "wb");
  if (!out) return false;
  char buffer[65536];
  bool ok = true;
  while (true) {
    ssize_t n = read(fd, buffer, sizeof buffer);
    if (n == 0) break;
    if (n < 0) {
      if (errno == EINTR) continue;
      ok = false;
      break;
    }
    if (std::fwrite(buffer, 1, (size_t)n, out) != (size_t)n) { ok = false; break; }
  }
  return std::fclose(out) == 0 && ok;
}

} // namespace

// ----------------------------------------------------------------------------- : NativeCore

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  JNIEnv* env = nullptr;
  if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
  if (!load_classes(env)) return JNI_ERR;
  return JNI_VERSION_1_6;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_init(JNIEnv* env, jobject, jstring template_dir, jstring font_store_dir,
                                             jstring program_data_dir, jstring locale) {
  F::InitOptions options;
  options.template_dir     = from_java(env, template_dir);
  options.font_store_dir   = from_java(env, font_store_dir);
  options.program_data_dir = from_java(env, program_data_dir);
  options.locale           = from_java(env, locale);
  F::Result r = guarded<F::Result>([&] {
    // the Android backends (ADR 0004), installed before the core starts
    set_image_codec(make_skia_image_codec());
    set_canvas_factory(make_unique<SkiaCanvasFactory>());
    return F::init(options);
  });
  return to_java(env, r);
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_shutdown(JNIEnv* env, jobject) {
  return to_java(env, guarded<F::Result>([] { return F::shutdown(); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_openSessionFd(JNIEnv* env, jobject, jint fd, jstring working_copy_dir) {
  std::string work = from_java(env, working_copy_dir);
  F::OpenResult r = guarded<F::OpenResult>([&] {
    if (work.empty()) return failed<F::OpenResult>("a working copy directory is needed", F::Status::InvalidArgument);
    // The facade opens Sets by path, and copies them into the working copy; the Set's bytes go
    // through a file next to it, which is gone once the Set is open
    std::string source = work + ".source.mse-set";
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(work).parent_path(), ec);
    if (!copy_fd_to_file(fd, source)) {
      unlink(source.c_str());
      return failed<F::OpenResult>("could not read the Set's file");
    }
    F::OpenResult open = F::openSession(source, work);
    unlink(source.c_str());
    return open;
  });
  jobject result = to_java(env, r);
  jobject out = env->NewObject(cls.open_result, cls.open_result_init, result, (jlong)r.session);
  env->DeleteLocalRef(result);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_closeSession(JNIEnv* env, jobject, jlong session) {
  return to_java(env, guarded<F::Result>([&] { return F::closeSession(session); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_listCards(JNIEnv* env, jobject, jlong session) {
  F::CardList r = guarded<F::CardList>([&] { return F::listCards(session); });
  std::vector<std::string> uids, names;
  for (const F::CardInfo& c : r.cards) {
    uids.push_back(c.uid);
    names.push_back(c.name);
  }
  jobject result = to_java(env, r);
  jobjectArray juids = to_java(env, uids), jnames = to_java(env, names);
  jobject out = env->NewObject(cls.card_list_result, cls.card_list_result_init, result, juids, jnames);
  env->DeleteLocalRef(result);
  env->DeleteLocalRef(juids);
  env->DeleteLocalRef(jnames);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_cardSize(JNIEnv* env, jobject, jlong session, jstring card_uid) {
  std::string uid = from_java(env, card_uid);
  F::CardSize r = guarded<F::CardSize>([&] { return F::cardSize(session, uid); });
  jobject result = to_java(env, r);
  jobject out = env->NewObject(cls.card_size_result, cls.card_size_result_init, result, (jdouble)r.width, (jdouble)r.height);
  env->DeleteLocalRef(result);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_renderCard(JNIEnv* env, jobject, jlong session, jstring card_uid, jobject bitmap) {
  std::string uid = from_java(env, card_uid);
  F::RenderResult r = guarded<F::RenderResult>([&] {
    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS
        || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
      return failed<F::RenderResult>("the Bitmap must be ARGB_8888", F::Status::InvalidArgument);
    }
    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
      return failed<F::RenderResult>("could not lock the Bitmap's pixels");
    }
    // ARGB_8888 is R, G, B, A in memory, premultiplied; renderCard's pixels are opaque, so the same
    F::RenderResult rr = F::renderCard(session, uid, (std::uint8_t*)pixels, (int)info.width, (int)info.height, (int)info.stride);
    AndroidBitmap_unlockPixels(env, bitmap);
    return rr;
  });
  jobjectArray regions = env->NewObjectArray((jsize)r.regions.size(), cls.field_region, nullptr);
  for (size_t i = 0 ; i < r.regions.size() ; ++i) {
    const F::FieldRegion& g = r.regions[i];
    jstring field = to_java(env, g.field);
    jobject jg = env->NewObject(cls.field_region, cls.field_region_init, field, g.x, g.y, g.width, g.height,
                                g.rotation, (jint)g.z, (jboolean)g.editable);
    env->SetObjectArrayElement(regions, (jsize)i, jg);
    env->DeleteLocalRef(jg);
    env->DeleteLocalRef(field);
  }
  jobject result = to_java(env, r);
  jobject out = env->NewObject(cls.render_result, cls.render_result_init, result, regions);
  env->DeleteLocalRef(result);
  env->DeleteLocalRef(regions);
  return out;
}

// ----------------------------------------------------------------------------- : Templates

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_listTemplates(JNIEnv* env, jobject) {
  F::TemplateList r = guarded<F::TemplateList>([] { return F::listTemplates(); });
  jobjectArray list = env->NewObjectArray((jsize)r.templates.size(), cls.template_info, nullptr);
  for (size_t i = 0 ; i < r.templates.size() ; ++i) {
    const F::TemplateInfo& t = r.templates[i];
    jstring name = to_java(env, t.name), type = to_java(env, t.type), full = to_java(env, t.full_name),
            version = to_java(env, t.version), game = to_java(env, t.game);
    jobject jt = env->NewObject(cls.template_info, cls.template_info_init, name, type, full, version, game);
    env->SetObjectArrayElement(list, (jsize)i, jt);
    for (jobject o : {(jobject)name, (jobject)type, (jobject)full, (jobject)version, (jobject)game, jt}) env->DeleteLocalRef(o);
  }
  jobject result = to_java(env, r);
  jobject out = env->NewObject(cls.template_list_result, cls.template_list_result_init, result, list);
  env->DeleteLocalRef(result);
  env->DeleteLocalRef(list);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_refreshTemplates(JNIEnv* env, jobject) {
  return to_java(env, guarded<F::Result>([] { return F::refreshTemplates(); }));
}

// Template import may run on any thread but the core thread's (facade.hpp); the Kotlin side runs
// it on a background thread.

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_scanTemplatesFd(JNIEnv* env, jobject, jint fd) {
  F::ImportScan r = guarded<F::ImportScan>([&] { return F::scanTemplates((int)fd); });
  jobjectArray packages = env->NewObjectArray((jsize)r.packages.size(), cls.scanned_package, nullptr);
  for (size_t i = 0 ; i < r.packages.size() ; ++i) {
    const F::ScannedPackage& p = r.packages[i];
    jstring name = to_java(env, p.name), type = to_java(env, p.type), full = to_java(env, p.full_name),
            version = to_java(env, p.version), installed = to_java(env, p.installed_version),
            group = to_java(env, p.installer_group), reason = to_java(env, p.reason);
    jobject jp = env->NewObject(cls.scanned_package, cls.scanned_package_init, name, type, full, version, installed,
                                group, (jlong)p.size, (jint)p.verdict, reason);
    env->SetObjectArrayElement(packages, (jsize)i, jp);
    for (jobject o : {(jobject)name, (jobject)type, (jobject)full, (jobject)version, (jobject)installed,
                      (jobject)group, (jobject)reason, jp}) env->DeleteLocalRef(o);
  }
  jobject result = to_java(env, r);
  jobject out = env->NewObject(cls.scan_result, cls.scan_result_init, result, packages, (jint)r.fonts.size(),
                               (jlong)r.fonts_size, (jint)r.new_fonts.size(), (jlong)r.new_fonts_size,
                               (jint)r.dictionaries.size(), (jint)r.new_dictionaries.size());
  env->DeleteLocalRef(result);
  env->DeleteLocalRef(packages);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_installTemplatesFd(JNIEnv* env, jobject, jint fd, jobjectArray names, jobject listener) {
  F::ImportSelection selection;
  for (jsize i = 0, n = env->GetArrayLength(names) ; i < n ; ++i) {
    jstring name = (jstring)env->GetObjectArrayElement(names, i);
    selection.packages.push_back(from_java(env, name));
    env->DeleteLocalRef(name);
  }
  // progress goes to the listener, on this (the calling) thread; it returns false to cancel
  F::ImportProgress progress;
  if (listener) {
    progress = [env, listener](std::uint64_t done, std::uint64_t total) {
      jboolean go_on = env->CallBooleanMethod(listener, cls.progress_on_progress, (jlong)done, (jlong)total);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
      }
      return go_on == JNI_TRUE;
    };
  }
  F::ImportResult r = guarded<F::ImportResult>([&] { return F::installTemplates((int)fd, selection, progress); });
  jobjectArray packages = env->NewObjectArray((jsize)r.packages.size(), cls.imported_package, nullptr);
  for (size_t i = 0 ; i < r.packages.size() ; ++i) {
    const F::ImportedPackage& p = r.packages[i];
    jstring name = to_java(env, p.name), version = to_java(env, p.version), error = to_java(env, p.error);
    jobject jp = env->NewObject(cls.imported_package, cls.imported_package_init, name, version, (jint)p.outcome,
                                (jboolean)p.dependency, error);
    env->SetObjectArrayElement(packages, (jsize)i, jp);
    for (jobject o : {(jobject)name, (jobject)version, (jobject)error, jp}) env->DeleteLocalRef(o);
  }
  jobject result = to_java(env, r);
  jobject out = env->NewObject(cls.import_result, cls.import_result_init, result, packages, (jint)r.fonts.size(),
                               (jint)r.dictionaries.size(), (jboolean)r.cancelled);
  env->DeleteLocalRef(result);
  env->DeleteLocalRef(packages);
  return out;
}

} // extern "C"
