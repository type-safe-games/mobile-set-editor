# Mobile Set Editor - R8 rules for the release build (#77)
# SPDX-License-Identifier: GPL-3.0-or-later

# The JNI layer (src/main/cpp/mse_jni.cpp) finds these classes, constructors and methods by name:
# the result classes it builds, NativeCore's native methods and the listener it calls back. Keep
# the whole package as it is rather than list each signature.
-keep class org.mobileseteditor.core.** { *; }
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}
