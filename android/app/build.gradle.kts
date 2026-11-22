// Mobile Set Editor - the Android app (ADR 0006)
// SPDX-License-Identifier: GPL-3.0-or-later

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}

/** The repository root: the core, the facade and every native dependency are built from there. */
val repoRoot: File = rootDir.parentFile
val abis = listOf("arm64-v8a", "x86_64")

// Versions (#77). versionName is the release tag without its "v": tag v0.1.0 is versionName 0.1.0,
// and the tag is made from this line, so a build of the tagged source needs no git. versionCode is
// per ABI, for F-Droid's per-ABI builds: (major * 10000 + minor * 100 + patch) * 10 + the ABI's
// digit (arm64-v8a 1, x86_64 2), so 0.1.0 is 1001 on arm64-v8a and 1002 on x86_64; an APK with
// every ABI (debug builds) has digit 0.
val appVersion = "0.1.0"
val abiDigits = mapOf("arm64-v8a" to 1, "x86_64" to 2)
/** A release build: one APK per ABI (splits); otherwise one APK with every ABI */
val releaseBuild = gradle.startParameter.taskNames.any { it.contains("Release", ignoreCase = true) }
val baseVersionCode = appVersion.split('.').map(String::toInt).let { (major, minor, patch) -> major * 10000 + minor * 100 + patch }

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
        versionCode = baseVersionCode * 10
        versionName = appVersion
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        if (!releaseBuild) ndk { abiFilters += abis } // (a release's splits choose the ABIs)
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static", "-DCMAKE_BUILD_TYPE=Release")
                // the same binary wherever the source is checked out (#77)
                arguments += "-DMSE_REPRODUCIBLE=ON"
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

    buildTypes {
        // F-Droid builds and signs releases itself: no signing config here (#77)
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            vcsInfo.include = false
            ndk { debugSymbolLevel = "NONE" }
        }
    }

    // One APK per ABI for releases (F-Droid builds each); debug builds keep every ABI in one
    splits {
        abi {
            isEnable = releaseBuild
            reset()
            include(*abis.toTypedArray())
            isUniversalApk = false
        }
    }

    // No dependency metadata blob in the APK: it is signed with a key only Google can read (F-Droid)
    dependenciesInfo {
        includeInApk = false
        includeInBundle = false
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
    implementation(libs.lifecycle.viewmodel.compose)
    implementation(libs.coroutines.android)
    testImplementation(libs.junit)
    testImplementation(libs.coroutines.test)
    androidTestImplementation(platform(libs.compose.bom))
    androidTestImplementation(libs.compose.ui.test.junit4)
    debugImplementation(libs.compose.ui.test.manifest)
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

// The program's own data, the English locale the core runs with (as the desktop does): the
// repository's data/en.mse-locale, as the assets' program-data/en.mse-locale
abstract class CopyProgramData : DefaultTask() {
    @get:InputDirectory
    abstract val locale: DirectoryProperty

    @get:OutputDirectory
    abstract val output: DirectoryProperty

    @TaskAction
    fun copy() {
        val out = output.get().asFile
        out.deleteRecursively()
        locale.get().asFile.copyRecursively(File(out, "program-data/en.mse-locale"))
    }
}
val copyProgramData = tasks.register<CopyProgramData>("copyProgramData") {
    locale.set(repoRoot.resolve("data/en.mse-locale"))
}
androidComponents {
    onVariants { variant ->
        variant.sources.assets?.addGeneratedSourceDirectory(copyProgramData, CopyProgramData::output)
        // per-ABI version codes (see appVersion)
        for (output in variant.outputs) {
            val abi = output.filters.find { it.filterType == com.android.build.api.variant.FilterConfiguration.FilterType.ABI }?.identifier
            output.versionCode.set(baseVersionCode * 10 + (abi?.let { abiDigits.getValue(it) } ?: 0))
        }
    }
}
