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
#include <cstdlib>
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
  jclass action_result, session_info, value_result, field_at_result, bytes_result, keyword_list_result, new_keyword_result, new_cards_result, link_type_list_result, symbol_menu_result, symbol_preview_result, field_list_result, text_result;
  jmethodID string_from_bytes, string_get_bytes;
  jmethodID message_init, result_init, open_result_init, card_list_result_init, card_size_result_init,
            field_region_init, render_result_init;
  jmethodID scanned_package_init, scan_result_init, imported_package_init, import_result_init,
            template_info_init, template_list_result_init, progress_on_progress;
  jmethodID action_result_init, session_info_init, value_result_init, field_at_result_init, bytes_result_init, keyword_list_result_init, new_keyword_result_init, new_cards_result_init, link_type_list_result_init, symbol_menu_result_init, symbol_preview_result_init, field_list_result_init, text_result_init;
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
  cls.action_result        = global_class(env, p + "ActionResult");
  cls.session_info         = global_class(env, p + "SessionInfo");
  cls.value_result         = global_class(env, p + "ValueResult");
  cls.field_at_result      = global_class(env, p + "FieldAtResult");
  cls.bytes_result         = global_class(env, p + "BytesResult");
  cls.keyword_list_result  = global_class(env, p + "KeywordListResult");
  cls.new_keyword_result   = global_class(env, p + "NewKeywordResult");
  cls.new_cards_result     = global_class(env, p + "NewCardsResult");
  cls.link_type_list_result = global_class(env, p + "LinkTypeListResult");
  cls.symbol_menu_result   = global_class(env, p + "SymbolMenuResult");
  cls.symbol_preview_result = global_class(env, p + "SymbolPreviewResult");
  cls.field_list_result    = global_class(env, p + "FieldListResult");
  cls.text_result          = global_class(env, p + "TextResult");
  if (env->ExceptionCheck()) return false;
  std::string result_sig = "L" + p + "CoreResult;";
  cls.string_from_bytes     = env->GetMethodID(cls.string, "<init>", "([BLjava/lang/String;)V");
  cls.string_get_bytes      = env->GetMethodID(cls.string, "getBytes", "(Ljava/lang/String;)[B");
  cls.message_init          = env->GetMethodID(cls.message, "<init>", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  cls.result_init           = env->GetMethodID(cls.result, "<init>", ("(ILjava/lang/String;Ljava/lang/String;[L" + p + "CoreMessage;)V").c_str());
  cls.open_result_init      = env->GetMethodID(cls.open_result, "<init>", ("(" + result_sig + "J)V").c_str());
  cls.card_list_result_init = env->GetMethodID(cls.card_list_result, "<init>", ("(" + result_sig + "[Ljava/lang/String;[Ljava/lang/String;[[Ljava/lang/String;[[Ljava/lang/String;[Ljava/lang/String;[Z[Z)V").c_str());
  cls.card_size_result_init = env->GetMethodID(cls.card_size_result, "<init>", ("(" + result_sig + "DD)V").c_str());
  cls.field_region_init     = env->GetMethodID(cls.field_region, "<init>", "(Ljava/lang/String;DDDDDIZ)V");
  cls.render_result_init    = env->GetMethodID(cls.render_result, "<init>", ("(" + result_sig + "[L" + p + "FieldRegion;)V").c_str());
  cls.scanned_package_init  = env->GetMethodID(cls.scanned_package, "<init>",
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JILjava/lang/String;[Ljava/lang/String;)V");
  cls.scan_result_init      = env->GetMethodID(cls.scan_result, "<init>", ("(" + result_sig + "[L" + p + "ScannedPackage;IJIJII)V").c_str());
  cls.imported_package_init = env->GetMethodID(cls.imported_package, "<init>", "(Ljava/lang/String;Ljava/lang/String;IZLjava/lang/String;)V");
  cls.import_result_init    = env->GetMethodID(cls.import_result, "<init>", ("(" + result_sig + "[L" + p + "ImportedPackage;IIZ)V").c_str());
  cls.template_info_init    = env->GetMethodID(cls.template_info, "<init>",
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  cls.template_list_result_init = env->GetMethodID(cls.template_list_result, "<init>", ("(" + result_sig + "[L" + p + "TemplateInfo;)V").c_str());
  cls.progress_on_progress  = env->GetMethodID(cls.progress_listener, "onProgress", "(JJ)Z");
  cls.action_result_init    = env->GetMethodID(cls.action_result, "<init>",
    ("(" + result_sig + "Z[Ljava/lang/String;ZZZZLjava/lang/String;Ljava/lang/String;)V").c_str());
  cls.session_info_init     = env->GetMethodID(cls.session_info, "<init>",
    ("(" + result_sig + "Ljava/lang/String;Ljava/lang/String;ZZZ)V").c_str());
  cls.symbol_menu_result_init = env->GetMethodID(cls.symbol_menu_result, "<init>",
    ("(" + result_sig + "ZZ[I[Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[I[I)V").c_str());
  cls.symbol_preview_result_init = env->GetMethodID(cls.symbol_preview_result, "<init>", ("(" + result_sig + "II[I)V").c_str());
  cls.link_type_list_result_init = env->GetMethodID(cls.link_type_list_result, "<init>", ("(" + result_sig + "[Ljava/lang/String;[Ljava/lang/String;)V").c_str());
  cls.new_cards_result_init = env->GetMethodID(cls.new_cards_result, "<init>", ("(L" + p + "ActionResult;[Ljava/lang/String;)V").c_str());
  cls.keyword_list_result_init = env->GetMethodID(cls.keyword_list_result, "<init>", ("(" + result_sig +
    "[J[Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[Z[Z[Ljava/lang/String;[I[Ljava/lang/String;[Ljava/lang/String;)V").c_str());
  cls.new_keyword_result_init = env->GetMethodID(cls.new_keyword_result, "<init>", ("(L" + p + "ActionResult;J)V").c_str());
  cls.bytes_result_init    = env->GetMethodID(cls.bytes_result, "<init>", ("(" + result_sig + "[B)V").c_str());
  cls.field_at_result_init  = env->GetMethodID(cls.field_at_result, "<init>", ("(" + result_sig + "Ljava/lang/String;)V").c_str());
  cls.value_result_init     = env->GetMethodID(cls.value_result, "<init>", ("(" + result_sig + "Ljava/lang/String;Ljava/lang/String;)V").c_str());
  cls.field_list_result_init = env->GetMethodID(cls.field_list_result, "<init>",
    ("(" + result_sig + "[Ljava/lang/String;[Ljava/lang/String;[Z[Z[[Ljava/lang/String;[[Ljava/lang/String;[[Ljava/lang/String;[Z)V").c_str());
  cls.text_result_init      = env->GetMethodID(cls.text_result, "<init>",
    ("(L" + p + "ActionResult;Ljava/lang/String;II[I[I)V").c_str());
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

jobject to_java(JNIEnv* env, const F::ActionResult& r) {
  jobject result = to_java(env, static_cast<const F::Result&>(r));
  jobjectArray uids = to_java(env, r.stale.card_uids);
  jstring undo = to_java(env, r.undo_name), redo = to_java(env, r.redo_name);
  jobject out = env->NewObject(cls.action_result, cls.action_result_init, result, (jboolean)r.stale.all, uids,
                               (jboolean)r.stale.card_list, (jboolean)r.dirty, (jboolean)r.can_undo, (jboolean)r.can_redo,
                               undo, redo);
  for (jobject o : {result, (jobject)uids, (jobject)undo, (jobject)redo}) env->DeleteLocalRef(o);
  return out;
}

jobject to_java(JNIEnv* env, const F::TextResult& r) {
  jobject action = to_java(env, static_cast<const F::ActionResult&>(r));
  jstring text = to_java(env, r.text);
  jsize n = (jsize)r.locked.size();
  std::vector<jint> starts(n), ends(n);
  for (jsize i = 0 ; i < n ; ++i) { starts[i] = r.locked[i].start; ends[i] = r.locked[i].end; }
  jintArray js = env->NewIntArray(n), je = env->NewIntArray(n);
  env->SetIntArrayRegion(js, 0, n, starts.data());
  env->SetIntArrayRegion(je, 0, n, ends.data());
  jobject out = env->NewObject(cls.text_result, cls.text_result_init, action, text, (jint)r.selection_start,
                               (jint)r.selection_end, js, je);
  for (jobject o : {action, (jobject)text, (jobject)js, (jobject)je}) env->DeleteLocalRef(o);
  return out;
}

/// A facade Target from the Kotlin side's kind (Target.Kind's order) and card uid
F::Target target_from(JNIEnv* env, jint kind, jstring card_uid) {
  F::Target t;
  t.kind = (F::Target::Kind)kind;
  t.card_uid = from_java(env, card_uid);
  // a keyword's id travels as the uid's string
  if (t.kind == F::Target::Kind::Keyword) {
    t.keyword_id = std::strtoll(t.card_uid.c_str(), nullptr, 10);
    t.card_uid.clear();
  }
  return t;
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
Java_org_mobileseteditor_core_NativeCore_newSession(JNIEnv* env, jobject, jstring game, jstring stylesheet, jstring working_copy_dir) {
  std::string g = from_java(env, game), st = from_java(env, stylesheet), work = from_java(env, working_copy_dir);
  F::OpenResult r = guarded<F::OpenResult>([&] { return F::newSession(g, st, work); });
  jobject result = to_java(env, r);
  jobject out = env->NewObject(cls.open_result, cls.open_result_init, result, (jlong)r.session);
  env->DeleteLocalRef(result);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_openWorkingCopy(JNIEnv* env, jobject, jstring working_copy_dir, jboolean unsaved) {
  std::string work = from_java(env, working_copy_dir);
  F::OpenResult r = guarded<F::OpenResult>([&] { return F::openWorkingCopy(work, unsaved == JNI_TRUE); });
  jobject result = to_java(env, r);
  jobject out = env->NewObject(cls.open_result, cls.open_result_init, result, (jlong)r.session);
  env->DeleteLocalRef(result);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_flush(JNIEnv* env, jobject, jlong session) {
  return to_java(env, guarded<F::Result>([&] { return F::flush(session); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_saveToFd(JNIEnv* env, jobject, jlong session, jint fd) {
  return to_java(env, guarded<F::ActionResult>([&] { return F::saveTo(session, (int)fd); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_sessionInfo(JNIEnv* env, jobject, jlong session) {
  F::SessionInfo r = guarded<F::SessionInfo>([&] { return F::sessionInfo(session); });
  jobject result = to_java(env, r);
  jstring game = to_java(env, r.game), stylesheet = to_java(env, r.stylesheet);
  jobject out = env->NewObject(cls.session_info, cls.session_info_init, result, game, stylesheet,
                               (jboolean)r.dirty, (jboolean)r.can_undo, (jboolean)r.can_redo);
  for (jobject o : {result, (jobject)game, (jobject)stylesheet}) env->DeleteLocalRef(o);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_getValue(JNIEnv* env, jobject, jlong session, jint kind, jstring card_uid, jstring field) {
  F::Target target = target_from(env, kind, card_uid);
  std::string f = from_java(env, field);
  F::ValueResult r = guarded<F::ValueResult>([&] { return F::getValue(session, target, f); });
  jobject result = to_java(env, r);
  jstring type = to_java(env, r.type), value = to_java(env, r.value);
  jobject out = env->NewObject(cls.value_result, cls.value_result_init, result, type, value);
  for (jobject o : {result, (jobject)type, (jobject)value}) env->DeleteLocalRef(o);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_setValue(JNIEnv* env, jobject, jlong session, jint kind, jstring card_uid,
                                                 jstring field, jstring value) {
  F::Target target = target_from(env, kind, card_uid);
  std::string f = from_java(env, field), v = from_java(env, value);
  return to_java(env, guarded<F::ActionResult>([&] { return F::setValue(session, target, f, v); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_listFields(JNIEnv* env, jobject, jlong session, jint kind, jstring card_uid) {
  F::Target target = target_from(env, kind, card_uid);
  F::FieldList r = guarded<F::FieldList>([&] { return F::listFields(session, target); });
  std::vector<std::string> names, types;
  jsize n = (jsize)r.fields.size();
  jbooleanArray editable = env->NewBooleanArray(n), multi = env->NewBooleanArray(n);
  jclass string_array = env->FindClass("[Ljava/lang/String;");
  jobjectArray choices = env->NewObjectArray(n, string_array, nullptr);
  jobjectArray color_names = env->NewObjectArray(n, string_array, nullptr);
  jobjectArray color_values = env->NewObjectArray(n, string_array, nullptr);
  jbooleanArray allow_custom = env->NewBooleanArray(n);
  for (jsize i = 0 ; i < n ; ++i) {
    const F::FieldInfo& f = r.fields[i];
    names.push_back(f.name);
    types.push_back(f.type);
    jboolean e = f.editable, m = f.multi_line;
    env->SetBooleanArrayRegion(editable, i, 1, &e);
    env->SetBooleanArrayRegion(multi, i, 1, &m);
    jobjectArray c = to_java(env, f.choices);
    env->SetObjectArrayElement(choices, i, c);
    env->DeleteLocalRef(c);
    std::vector<std::string> cn, cv;
    for (const F::ColorChoice& cc : f.colors) { cn.push_back(cc.name); cv.push_back(cc.color); }
    jobjectArray jcn = to_java(env, cn), jcv = to_java(env, cv);
    env->SetObjectArrayElement(color_names, i, jcn);
    env->SetObjectArrayElement(color_values, i, jcv);
    env->DeleteLocalRef(jcn);
    env->DeleteLocalRef(jcv);
    jboolean ac = f.allow_custom;
    env->SetBooleanArrayRegion(allow_custom, i, 1, &ac);
  }
  jobject result = to_java(env, r);
  jobjectArray jn = to_java(env, names), jt = to_java(env, types);
  jobject out = env->NewObject(cls.field_list_result, cls.field_list_result_init, result, jn, jt, editable, multi, choices,
                               color_names, color_values, allow_custom);
  for (jobject o : {result, (jobject)jn, (jobject)jt, (jobject)editable, (jobject)multi, (jobject)choices,
                    (jobject)color_names, (jobject)color_values, (jobject)allow_custom, (jobject)string_array}) env->DeleteLocalRef(o);
  return out;
}

/// The menu's items in depth-first order, each with its parent's index (-1 at the top)
static void flatten(const std::vector<F::SymbolMenuItem>& items, int parent, std::vector<const F::SymbolMenuItem*>& out, std::vector<jint>& parents) {
  for (const F::SymbolMenuItem& i : items) {
    int index = (int)out.size();
    out.push_back(&i);
    parents.push_back(parent);
    flatten(i.items, index, out, parents);
  }
}

static jintArray to_java_ints(JNIEnv* env, const std::vector<jint>& v) {
  jintArray a = env->NewIntArray((jsize)v.size());
  if (!v.empty()) env->SetIntArrayRegion(a, 0, (jsize)v.size(), v.data());
  return a;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_symbolMenu(JNIEnv* env, jobject, jlong session, jint kind, jstring card_uid, jstring field) {
  F::Target target = target_from(env, kind, card_uid);
  std::string f = from_java(env, field);
  F::SymbolMenu r = guarded<F::SymbolMenu>([&] { return F::symbolMenu(session, target, f); });
  std::vector<const F::SymbolMenuItem*> items;
  std::vector<jint> parents, types, ids;
  flatten(r.items, -1, items, parents);
  std::vector<std::string> labels, codes, prompts;
  for (const F::SymbolMenuItem* i : items) {
    types.push_back((jint)i->type);
    ids.push_back(i->id);
    labels.push_back(i->label);
    codes.push_back(i->code);
    prompts.push_back(i->prompt);
  }
  jobject result = to_java(env, r);
  jintArray jtypes = to_java_ints(env, types), jids = to_java_ints(env, ids), jparents = to_java_ints(env, parents);
  jobjectArray jlabels = to_java(env, labels), jcodes = to_java(env, codes), jprompts = to_java(env, prompts);
  jobject out = env->NewObject(cls.symbol_menu_result, cls.symbol_menu_result_init, result, (jboolean)r.has_symbols,
                               (jboolean)r.always_symbol, jtypes, jlabels, jcodes, jprompts, jids, jparents);
  for (jobject o : {result, (jobject)jtypes, (jobject)jids, (jobject)jparents, (jobject)jlabels, (jobject)jcodes, (jobject)jprompts}) env->DeleteLocalRef(o);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_symbolPreview(JNIEnv* env, jobject, jlong session, jint kind, jstring card_uid,
                                                      jstring field, jint item_id, jint size) {
  F::Target target = target_from(env, kind, card_uid);
  std::string f = from_java(env, field);
  F::ImageData r = guarded<F::ImageData>([&] { return F::symbolPreview(session, target, f, item_id, size); });
  // as Android's ARGB ints
  std::vector<jint> argb((size_t)r.width * r.height);
  for (size_t i = 0 ; i < argb.size() ; ++i) {
    const std::uint8_t* p = &r.rgba[4 * i];
    argb[i] = (jint)((std::uint32_t)p[3] << 24 | (std::uint32_t)p[0] << 16 | (std::uint32_t)p[1] << 8 | p[2]);
  }
  jobject result = to_java(env, r);
  jintArray pixels = to_java_ints(env, argb);
  jobject out = env->NewObject(cls.symbol_preview_result, cls.symbol_preview_result_init, result, (jint)r.width, (jint)r.height, pixels);
  env->DeleteLocalRef(result);
  env->DeleteLocalRef(pixels);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_getText(JNIEnv* env, jobject, jlong session, jint kind, jstring card_uid, jstring field) {
  F::Target target = target_from(env, kind, card_uid);
  std::string f = from_java(env, field);
  return to_java(env, guarded<F::TextResult>([&] { return F::getText(session, target, f); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_replaceRange(JNIEnv* env, jobject, jlong session, jint kind, jstring card_uid,
                                                     jstring field, jint start, jint end, jstring text, jboolean as_symbol) {
  F::Target target = target_from(env, kind, card_uid);
  std::string f = from_java(env, field), t = from_java(env, text);
  return to_java(env, guarded<F::TextResult>([&] {
    return F::replaceRange(session, target, f, (int)start, (int)end, t, as_symbol == JNI_TRUE);
  }));
}

/// A Java String[] as uids
static std::vector<std::string> from_java_strings(JNIEnv* env, jobjectArray a) {
  std::vector<std::string> out;
  jsize n = a ? env->GetArrayLength(a) : 0;
  for (jsize i = 0 ; i < n ; ++i) {
    jstring s = (jstring)env->GetObjectArrayElement(a, i);
    out.push_back(from_java(env, s));
    env->DeleteLocalRef(s);
  }
  return out;
}

static jobject new_cards_to_java(JNIEnv* env, const F::NewCardsResult& r) {
  jobject action = to_java(env, static_cast<const F::ActionResult&>(r));
  jobjectArray uids = to_java(env, r.uids);
  jobject out = env->NewObject(cls.new_cards_result, cls.new_cards_result_init, action, uids);
  env->DeleteLocalRef(action);
  env->DeleteLocalRef(uids);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_addCard(JNIEnv* env, jobject, jlong session, jint position) {
  return new_cards_to_java(env, guarded<F::NewCardsResult>([&] { return F::addCard(session, (int)position); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_duplicateCards(JNIEnv* env, jobject, jlong session, jobjectArray card_uids) {
  std::vector<std::string> uids = from_java_strings(env, card_uids);
  return new_cards_to_java(env, guarded<F::NewCardsResult>([&] { return F::duplicateCards(session, uids); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_removeCards(JNIEnv* env, jobject, jlong session, jobjectArray card_uids) {
  std::vector<std::string> uids = from_java_strings(env, card_uids);
  return to_java(env, guarded<F::ActionResult>([&] { return F::removeCards(session, uids); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_moveCards(JNIEnv* env, jobject, jlong session, jobjectArray card_uids, jint position) {
  std::vector<std::string> uids = from_java_strings(env, card_uids);
  return to_java(env, guarded<F::ActionResult>([&] { return F::moveCards(session, uids, (int)position); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_listKeywords(JNIEnv* env, jobject, jlong session) {
  F::KeywordList r = guarded<F::KeywordList>([&] { return F::listKeywords(session); });
  jsize n = (jsize)r.keywords.size();
  std::vector<jlong> ids;
  std::vector<std::string> names, matches, reminders, rules, modes, errors;
  jbooleanArray from_game = env->NewBooleanArray(n), valid = env->NewBooleanArray(n);
  std::vector<jint> usage;
  for (jsize i = 0 ; i < n ; ++i) {
    const F::KeywordInfo& k = r.keywords[i];
    ids.push_back((jlong)k.id);
    names.push_back(k.keyword); matches.push_back(k.match); reminders.push_back(k.reminder);
    rules.push_back(k.rules); modes.push_back(k.mode); errors.push_back(k.reminder_error);
    usage.push_back(k.usage);
    jboolean g = k.from_game, v = k.valid;
    env->SetBooleanArrayRegion(from_game, i, 1, &g);
    env->SetBooleanArrayRegion(valid, i, 1, &v);
  }
  jlongArray jids = env->NewLongArray(n);
  if (n) env->SetLongArrayRegion(jids, 0, n, ids.data());
  jintArray jusage = env->NewIntArray(n);
  if (n) env->SetIntArrayRegion(jusage, 0, n, usage.data());
  // and the Game's parameter types, for inserting parameters
  F::KeywordParamTypeList types = guarded<F::KeywordParamTypeList>([&] { return F::listKeywordParamTypes(session); });
  std::vector<std::string> type_names, type_descriptions;
  for (const F::KeywordParamTypeInfo& t : types.types) { type_names.push_back(t.name); type_descriptions.push_back(t.description); }
  jobject result = to_java(env, r);
  jobjectArray a = to_java(env, names), b = to_java(env, matches), c = to_java(env, reminders), d = to_java(env, rules),
               e = to_java(env, modes), f = to_java(env, errors), g = to_java(env, type_names), h = to_java(env, type_descriptions);
  jobject out = env->NewObject(cls.keyword_list_result, cls.keyword_list_result_init, result, jids, a, b, c, d, e,
                               from_game, valid, f, jusage, g, h);
  for (jobject o : {result, (jobject)jids, (jobject)a, (jobject)b, (jobject)c, (jobject)d, (jobject)e, (jobject)f, (jobject)g, (jobject)h,
                    (jobject)from_game, (jobject)valid, (jobject)jusage}) env->DeleteLocalRef(o);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_addKeyword(JNIEnv* env, jobject, jlong session) {
  F::NewKeywordResult r = guarded<F::NewKeywordResult>([&] { return F::addKeyword(session); });
  jobject action = to_java(env, static_cast<const F::ActionResult&>(r));
  jobject out = env->NewObject(cls.new_keyword_result, cls.new_keyword_result_init, action, (jlong)r.id);
  env->DeleteLocalRef(action);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_removeKeyword(JNIEnv* env, jobject, jlong session, jlong keyword) {
  return to_java(env, guarded<F::ActionResult>([&] { return F::removeKeyword(session, (F::KeywordId)keyword); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_insertKeywordParameter(JNIEnv* env, jobject, jlong session, jlong keyword,
                                                               jint start, jint end, jstring type) {
  std::string t = from_java(env, type);
  return to_java(env, guarded<F::TextResult>([&] { return F::insertKeywordParameter(session, (F::KeywordId)keyword, start, end, t); }));
}

/// A card as the finished PNG; zoom <= 0 and bleed < 0 leave those to the desktop's defaults
JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_exportCardImage(JNIEnv* env, jobject, jlong session, jstring card_uid, jdouble zoom,
                                                        jint bleed, jboolean metadata) {
  std::string uid = from_java(env, card_uid);
  F::ExportOptions options;
  if (zoom > 0) options.zoom = (double)zoom;
  if (bleed >= 0) options.bleed = bleed != 0;
  options.metadata = metadata == JNI_TRUE;
  F::BytesResult r = guarded<F::BytesResult>([&] { return F::exportCardImage(session, uid, options); });
  jobject result = to_java(env, r);
  jbyteArray data = env->NewByteArray((jsize)r.data.size());
  if (!r.data.empty()) env->SetByteArrayRegion(data, 0, (jsize)r.data.size(), (const jbyte*)r.data.data());
  jobject out = env->NewObject(cls.bytes_result, cls.bytes_result_init, result, data);
  env->DeleteLocalRef(result);
  env->DeleteLocalRef(data);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_setCardStyle(JNIEnv* env, jobject, jlong session, jstring card_uid, jstring stylesheet) {
  std::string uid = from_java(env, card_uid), st = from_java(env, stylesheet);
  return to_java(env, guarded<F::ActionResult>([&] { return F::setCardStyle(session, uid, st); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_setSetStyle(JNIEnv* env, jobject, jlong session, jstring stylesheet) {
  std::string st = from_java(env, stylesheet);
  return to_java(env, guarded<F::ActionResult>([&] { return F::setSetStyle(session, st); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_setCardOwnStyling(JNIEnv* env, jobject, jlong session, jstring card_uid, jboolean own) {
  std::string uid = from_java(env, card_uid);
  return to_java(env, guarded<F::ActionResult>([&] { return F::setCardOwnStyling(session, uid, own == JNI_TRUE); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_listLinkTypes(JNIEnv* env, jobject, jlong session) {
  F::LinkTypeList r = guarded<F::LinkTypeList>([&] { return F::listLinkTypes(session); });
  std::vector<std::string> selected, linked;
  for (const F::LinkType& t : r.types) { selected.push_back(t.selected); linked.push_back(t.linked); }
  jobject result = to_java(env, r);
  jobjectArray js = to_java(env, selected), jl = to_java(env, linked);
  jobject out = env->NewObject(cls.link_type_list_result, cls.link_type_list_result_init, result, js, jl);
  for (jobject o : {result, (jobject)js, (jobject)jl}) env->DeleteLocalRef(o);
  return out;
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_link(JNIEnv* env, jobject, jlong session, jstring card_uid, jstring other_uid,
                                             jstring relation, jstring other_relation) {
  std::string a = from_java(env, card_uid), b = from_java(env, other_uid);
  std::string ra = from_java(env, relation), rb = from_java(env, other_relation);
  return to_java(env, guarded<F::ActionResult>([&] { return F::link(session, a, b, ra, rb); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_unlink(JNIEnv* env, jobject, jlong session, jstring card_uid, jstring other_uid) {
  std::string a = from_java(env, card_uid), b = from_java(env, other_uid);
  return to_java(env, guarded<F::ActionResult>([&] { return F::unlink(session, a, b); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_undo(JNIEnv* env, jobject, jlong session) {
  return to_java(env, guarded<F::ActionResult>([&] { return F::undo(session); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_redo(JNIEnv* env, jobject, jlong session) {
  return to_java(env, guarded<F::ActionResult>([&] { return F::redo(session); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_closeSession(JNIEnv* env, jobject, jlong session) {
  return to_java(env, guarded<F::Result>([&] { return F::closeSession(session); }));
}

JNIEXPORT jobject JNICALL
Java_org_mobileseteditor_core_NativeCore_listCards(JNIEnv* env, jobject, jlong session) {
  F::CardList r = guarded<F::CardList>([&] { return F::listCards(session); });
  std::vector<std::string> uids, names, stylesheets;
  jbooleanArray own_stylesheet = env->NewBooleanArray((jsize)r.cards.size()), own_styling = env->NewBooleanArray((jsize)r.cards.size());
  jclass string_array = env->FindClass("[Ljava/lang/String;");
  jobjectArray link_uids = env->NewObjectArray((jsize)r.cards.size(), string_array, nullptr);
  jobjectArray link_relations = env->NewObjectArray((jsize)r.cards.size(), string_array, nullptr);
  for (size_t i = 0 ; i < r.cards.size() ; ++i) {
    const F::CardInfo& c = r.cards[i];
    uids.push_back(c.uid);
    names.push_back(c.name);
    stylesheets.push_back(c.stylesheet);
    jboolean os = c.own_stylesheet, ost = c.own_styling;
    env->SetBooleanArrayRegion(own_stylesheet, (jsize)i, 1, &os);
    env->SetBooleanArrayRegion(own_styling, (jsize)i, 1, &ost);
    std::vector<std::string> lu, lr;
    for (const F::CardLinkInfo& l : c.links) { lu.push_back(l.uid); lr.push_back(l.relation); }
    jobjectArray jlu = to_java(env, lu), jlr = to_java(env, lr);
    env->SetObjectArrayElement(link_uids, (jsize)i, jlu);
    env->SetObjectArrayElement(link_relations, (jsize)i, jlr);
    env->DeleteLocalRef(jlu);
    env->DeleteLocalRef(jlr);
  }
  jobject result = to_java(env, r);
  jobjectArray juids = to_java(env, uids), jnames = to_java(env, names);
  jobjectArray jstyles = to_java(env, stylesheets);
  jobject out = env->NewObject(cls.card_list_result, cls.card_list_result_init, result, juids, jnames, link_uids, link_relations,
                               jstyles, own_stylesheet, own_styling);
  for (jobject o : {result, (jobject)juids, (jobject)jnames, (jobject)link_uids, (jobject)link_relations, (jobject)string_array,
                    (jobject)jstyles, (jobject)own_stylesheet, (jobject)own_styling}) env->DeleteLocalRef(o);
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
Java_org_mobileseteditor_core_NativeCore_fieldAt(JNIEnv* env, jobject, jlong session, jstring card_uid, jdouble x, jdouble y, jint width) {
  std::string uid = from_java(env, card_uid);
  F::FieldAt r = guarded<F::FieldAt>([&] { return F::fieldAt(session, uid, x, y, (int)width); });
  jobject result = to_java(env, r);
  jstring field = to_java(env, r.field);
  jobject out = env->NewObject(cls.field_at_result, cls.field_at_result_init, result, field);
  for (jobject o : {result, (jobject)field}) env->DeleteLocalRef(o);
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
            version = to_java(env, t.version), game = to_java(env, t.game), short_name = to_java(env, t.short_name),
            icon = to_java(env, t.icon_path);
    jobject jt = env->NewObject(cls.template_info, cls.template_info_init, name, type, full, version, game, short_name, icon);
    env->SetObjectArrayElement(list, (jsize)i, jt);
    for (jobject o : {(jobject)name, (jobject)type, (jobject)full, (jobject)version, (jobject)game, (jobject)short_name, (jobject)icon, jt}) env->DeleteLocalRef(o);
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
    std::vector<std::string> deps;
    for (const F::ImportDependency& d : p.dependencies) deps.push_back(d.package);
    jobjectArray jdeps = to_java(env, deps);
    jobject jp = env->NewObject(cls.scanned_package, cls.scanned_package_init, name, type, full, version, installed,
                                group, (jlong)p.size, (jint)p.verdict, reason, jdeps);
    env->SetObjectArrayElement(packages, (jsize)i, jp);
    for (jobject o : {(jobject)name, (jobject)type, (jobject)full, (jobject)version, (jobject)installed,
                      (jobject)group, (jobject)reason, (jobject)jdeps, jp}) env->DeleteLocalRef(o);
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
Java_org_mobileseteditor_core_NativeCore_installTemplatesFd(JNIEnv* env, jobject, jint fd, jobjectArray names,
                                                           jboolean fonts, jboolean dictionaries, jobject listener) {
  F::ImportSelection selection;
  selection.fonts = fonts == JNI_TRUE;
  selection.dictionaries = dictionaries == JNI_TRUE;
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
