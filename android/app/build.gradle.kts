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
    // After the GitHub owner type-safe-games (#53), with "_" for "-", which package names can't have.
    // The applicationId is permanent once on F-Droid (#76).
    namespace = "io.github.type_safe_games.mobileseteditor"
    compileSdk = 36
    buildToolsVersion = "36.1.0"
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "io.github.type_safe_games.mobileseteditor"
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
        // a debug build installs beside a release one
        debug {
            applicationIdSuffix = ".debug"
        }
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
    implementation(libs.compose.material.icons.core)
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
// The licences screen (#76): everything in the APK that isn't ours, with its licence text. The
// native components' texts are copied from their pinned submodules, so they are the ones built;
// the Java libraries come from the variant's runtime classpath.
/** A native component: its name, what it is, its licence, where its source is, its licence files
 *  (relative to the repository) and the third_party submodules it covers */
data class NativeComponent(val name: String, val what: String, val licence: String, val source: String, val files: List<String>, val covers: List<String>)
val nativeComponents = listOf(
    NativeComponent("Skia", "2D graphics", "BSD-3-Clause", "https://skia.org",
        listOf("third_party/skia/LICENSE"), listOf("third_party/skia")),
    NativeComponent("zlib", "compression, as Skia uses it", "Zlib", "https://chromium.googlesource.com/chromium/src/third_party/zlib",
        listOf("third_party/skia-deps/zlib/LICENSE"), listOf("third_party/skia-deps/zlib")),
    NativeComponent("libpng", "PNG images", "libpng-2.0", "http://www.libpng.org/pub/png/libpng.html",
        listOf("third_party/skia-deps/libpng/LICENSE"), listOf("third_party/skia-deps/libpng")),
    NativeComponent("libjpeg-turbo", "JPEG images", "IJG AND BSD-3-Clause AND Zlib", "https://libjpeg-turbo.org",
        listOf("third_party/skia-deps/libjpeg-turbo/LICENSE.md", "third_party/skia-deps/libjpeg-turbo/README.ijg"), listOf("third_party/skia-deps/libjpeg-turbo")),
    NativeComponent("FreeType", "font rendering. Portions of this software are copyright © The FreeType Project (www.freetype.org). All rights reserved.", "FTL", "https://freetype.org",
        listOf("third_party/skia-deps/freetype/LICENSE.TXT", "third_party/skia-deps/freetype/docs/FTL.TXT"), listOf("third_party/skia-deps/freetype")),
    NativeComponent("HarfBuzz", "text shaping", "MIT-Modern-Variant", "https://harfbuzz.github.io",
        listOf("third_party/skia-deps/harfbuzz/COPYING"), listOf("third_party/skia-deps/harfbuzz")),
    NativeComponent("Boost", "C++ libraries (header-only)", "BSL-1.0", "https://www.boost.org",
        listOf("third_party/boost/json/LICENSE_1_0.txt"), listOf("third_party/boost")),
    NativeComponent("Hunspell", "spell checking; used under the GPL", "MPL-1.1 OR GPL-2.0-or-later OR LGPL-2.1-or-later", "https://hunspell.github.io",
        listOf("third_party/hunspell/license.hunspell", "third_party/hunspell/COPYING", "third_party/hunspell/COPYING.MPL"), listOf("third_party/hunspell")),
    NativeComponent("minizip-ng", "zip files", "Zlib", "https://github.com/zlib-ng/minizip-ng",
        listOf("third_party/minizip-ng/LICENSE"), listOf("third_party/minizip-ng")),
)
/** Submodules that are build tools only: nothing from them is in the APK */
val buildToolSubmodules = setOf("third_party/gn")
/** Groups of Java libraries known to be Apache-2.0. A library from any other group fails the
 *  build until its licence is added here and on the screen. */
val apacheGroups = Regex("""androidx(\..+)?|org\.jetbrains(\.kotlinx?)?|org\.jspecify|com\.google\.guava""")

abstract class CollectLicences : DefaultTask() {
    /** One line per native component: name, what, licence, source and its files, tab-separated */
    @get:Input
    abstract val components: ListProperty<String>

    @get:InputFiles
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val licenceFiles: ConfigurableFileCollection

    /** group:name:version of each Java library in the APK */
    @get:Input
    abstract val libraries: ListProperty<String>

    @get:InputFile
    @get:PathSensitive(PathSensitivity.NONE)
    abstract val apache: RegularFileProperty

    @get:InputFile
    @get:PathSensitive(PathSensitivity.NONE)
    abstract val gpl: RegularFileProperty

    @get:Internal
    abstract val repo: DirectoryProperty

    @get:OutputDirectory
    abstract val output: DirectoryProperty

    @TaskAction
    fun collect() {
        val out = File(output.get().asFile, "licences")
        out.parentFile.deleteRecursively()
        out.mkdirs()
        val index = StringBuilder()
        // this app: name, what, licence, source, files (each file is copied under licences/<n>/)
        val app = "Mobile Set Editor\tthis app\tGPL-3.0-or-later\thttps://github.com/type-safe-games/mobile-set-editor"
        gpl.get().asFile.copyTo(File(out, "0/COPYING"))
        index.append("$app\tCOPYING\n")
        components.get().forEachIndexed { i, line ->
            val fields = line.split('\t')
            val files = fields[4].split(';').map { File(repo.get().asFile, it) }
            files.forEach { it.copyTo(File(out, "${i + 1}/${it.name}")) }
            index.append(fields.take(4).joinToString("\t")).append('\t').append(files.joinToString(";") { it.name }).append('\n')
        }
        val n = components.get().size + 1
        apache.get().asFile.copyTo(File(out, "$n/LICENSE-2.0.txt"))
        index.append("AndroidX, Kotlin and their libraries\tthe user interface (the list below)\tApache-2.0\thttps://developer.android.com/jetpack/androidx\tLICENSE-2.0.txt\n")
        File(out, "index.tsv").writeText(index.toString())
        File(out, "java-libraries.txt").writeText(libraries.get().joinToString("\n", postfix = "\n"))
    }
}

/** The third_party submodules, from .gitmodules */
val thirdPartySubmodules: List<String> = repoRoot.resolve(".gitmodules").readLines()
    .mapNotNull { Regex("""^\s*path\s*=\s*(third_party/\S+)""").find(it)?.groupValues?.get(1) }
run {
    val uncovered = thirdPartySubmodules.filter { sub ->
        sub !in buildToolSubmodules && nativeComponents.none { c -> c.covers.any { sub == it || sub.startsWith("$it/") } }
    }
    if (uncovered.isNotEmpty()) throw GradleException("No licence on the licences screen for ${uncovered.joinToString()} (#76): add it to nativeComponents")
}

/** Every module (group:name:version) a resolved dependency graph reaches */
fun modules(root: org.gradle.api.artifacts.result.ResolvedComponentResult): List<String> {
    val seen = mutableSetOf<org.gradle.api.artifacts.component.ComponentIdentifier>()
    val out = sortedSetOf<String>()
    fun walk(c: org.gradle.api.artifacts.result.ResolvedComponentResult) {
        if (!seen.add(c.id)) return
        (c.id as? org.gradle.api.artifacts.component.ModuleComponentIdentifier)?.let { out += "${it.group}:${it.module}:${it.version}" }
        c.dependencies.filterIsInstance<org.gradle.api.artifacts.result.ResolvedDependencyResult>().forEach { walk(it.selected) }
    }
    walk(root)
    return out.toList()
}

androidComponents {
    onVariants { variant ->
        variant.sources.assets?.addGeneratedSourceDirectory(copyProgramData, CopyProgramData::output)
        val licences = tasks.register<CollectLicences>("collect${variant.name.replaceFirstChar(Char::uppercase)}Licences") {
            components.set(nativeComponents.map { listOf(it.name, it.what, it.licence, it.source, it.files.joinToString(";")).joinToString("\t") })
            licenceFiles.from(nativeComponents.flatMap { c -> c.files.map { repoRoot.resolve(it) } })
            libraries.set(variant.runtimeConfiguration.incoming.resolutionResult.rootComponent.map { root ->
                val libs = modules(root)
                val other = libs.filterNot { apacheGroups.matches(it.substringBefore(':')) }
                if (other.isNotEmpty()) throw GradleException("Java libraries with no licence on the licences screen (#76): ${other.joinToString()}")
                libs
            })
            apache.set(layout.projectDirectory.file("licences/Apache-2.0.txt"))
            gpl.set(repoRoot.resolve("COPYING"))
            repo.set(repoRoot)
        }
        variant.sources.assets?.addGeneratedSourceDirectory(licences, CollectLicences::output)
        // per-ABI version codes (see appVersion)
        for (output in variant.outputs) {
            val abi = output.filters.find { it.filterType == com.android.build.api.variant.FilterConfiguration.FilterType.ABI }?.identifier
            output.versionCode.set(baseVersionCode * 10 + (abi?.let { abiDigits.getValue(it) } ?: 0))
        }
    }
}
