// Mobile Set Editor - the Android app (ADR 0006)
// SPDX-License-Identifier: GPL-3.0-or-later

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}

/** The repository root: the core, the facade and every native dependency are built from there. */
val repoRoot: File = rootDir.parentFile
val abis = listOf("arm64-v8a", "x86_64")

android {
    // Provisional names; the app is named in #76 (and keeps "Magic" out of them).
    namespace = "org.mobileseteditor"
    compileSdk = 36
    buildToolsVersion = "36.1.0"
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "org.mobileseteditor"
        minSdk = 26 // ADR 0006
        targetSdk = 36
        versionCode = 1
        versionName = "0.1"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk { abiFilters += abis }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static", "-DCMAKE_BUILD_TYPE=Release")
                targets += "mse" // libmse.so: the core, the facade and the JNI layer
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            // A CMake on PATH (the system's, or F-Droid's) instead of downloading the SDK's
            systemCMakeVersion()?.let { version = it }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures { compose = true }

    // The Test fixture is a test asset only, never in the app's APK (ADR 0008)
    sourceSets.getByName("androidTest").assets.srcDir(repoRoot.resolve("test/fixture"))
}

kotlin {
    compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) }
}

dependencies {
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.material3)
    implementation(libs.activity.compose)
    implementation(libs.coroutines.android)
    androidTestImplementation(libs.androidx.test.runner)
    androidTestImplementation(libs.androidx.test.junit)
}

/** The version of the CMake on PATH, if there is one */
fun systemCMakeVersion(): String? = try {
    val out = providers.exec { commandLine("cmake", "--version") }.standardOutput.asText.get()
    Regex("""cmake version (\d+\.\d+\.\d+)""").find(out)?.groupValues?.get(1)
} catch (e: Exception) {
    null
}

// Skia, built from its pinned submodules with tools/skia/build.sh before CMake runs (ADR 0011)
val skiaTasks = abis.map { abi ->
    tasks.register<Exec>("buildSkia-$abi") {
        group = "build"
        description = "Builds Skia for $abi with tools/skia/build.sh"
        workingDir = repoRoot
        commandLine("tools/skia/build.sh", "android-$abi")
        val ndk = androidComponents.sdkComponents.ndkDirectory
        doFirst { environment("ANDROID_NDK_HOME", ndk.get().asFile.absolutePath) }
    }
}
tasks.matching { it.name.startsWith("configureCMake") || it.name.startsWith("buildCMake") }.configureEach {
    dependsOn(skiaTasks)
}
