// Mobile Set Editor - what the core's calls return (ADR 0009)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.core

/** Mirrors the facade's Status (facade/facade.hpp), in the same order */
enum class Status { Ok, Failed, MissingTemplate, NotFound, InvalidArgument, WrongThread }

/** Mirrors the facade's Severity */
enum class Severity { Info, Warning, Error, Fatal }

/** A message the core queued during a call */
class CoreMessage(severity: Int, val text: String, val cardUid: String, val field: String) {
    val severity: Severity = Severity.entries[severity]
    override fun toString() = "$severity: $text"
}

/** What every call returns: its status and its messages. Built by the JNI layer. */
class CoreResult(status: Int, val error: String, val missingPackage: String, val messages: Array<CoreMessage>) {
    val status: Status = Status.entries[status]
    val ok: Boolean get() = status == Status.Ok
    override fun toString() = if (ok) "Ok" else "$status: $error"
}

class OpenResult(val result: CoreResult, val session: Long)

class CardListResult(val result: CoreResult, val uids: Array<String>, val names: Array<String>)

class CardSizeResult(val result: CoreResult, val width: Double, val height: Double)

/** A field's region in a rendered card, in bitmap pixels */
class FieldRegion(
    val field: String,
    val x: Double, val y: Double, val width: Double, val height: Double,
    val rotation: Double,
    val z: Int,
    val editable: Boolean,
)

class RenderResult(val result: CoreResult, val regions: Array<FieldRegion>)

// ----------------------------------------------------------------------------- Templates

/** An installed Template */
class TemplateInfo(val name: String, val type: String, val fullName: String, val version: String, val game: String)

class TemplateListResult(val result: CoreResult, val templates: Array<TemplateInfo>)

// ----------------------------------------------------------------------------- Template import (ADR 0005)

/** What installing a package found in a zip would do; mirrors the facade's Verdict */
enum class Verdict { Install, Replace, SkipNotNewer, Refuse }

class ScannedPackage(
    val name: String,
    val type: String,
    val fullName: String,
    val version: String,
    val installedVersion: String,
    val installerGroup: String,
    /** bytes when extracted */
    val size: Long,
    verdict: Int,
    /** why it is refused */
    val reason: String,
) {
    val verdict: Verdict = Verdict.entries[verdict]
    val installable: Boolean get() = verdict == Verdict.Install || verdict == Verdict.Replace
}

class ScanResult(
    val result: CoreResult,
    val packages: Array<ScannedPackage>,
    /** the loose fonts in the zip, and their bytes */
    val fontCount: Int,
    val fontsSize: Long,
    /** those of them the font store doesn't have yet (the others are skipped) */
    val newFontCount: Int,
    val newFontsSize: Long,
    val dictionaryCount: Int,
    val newDictionaryCount: Int,
) {
    /** What installing everything installable would extract */
    val installSize: Long get() = packages.filter { it.installable }.sumOf { it.size } + newFontsSize

    /** Would installing change anything? */
    val anythingNew: Boolean get() = packages.any { it.installable } || newFontCount > 0 || newDictionaryCount > 0
}

/** What happened to a package; mirrors the facade's Outcome */
enum class Outcome { Installed, Replaced, SkippedNotNewer, Refused, Failed, Cancelled }

class ImportedPackage(val name: String, val version: String, outcome: Int, val dependency: Boolean, val error: String) {
    val outcome: Outcome = Outcome.entries[outcome]
}

class ImportResult(
    val result: CoreResult,
    val packages: Array<ImportedPackage>,
    val fontCount: Int,
    val dictionaryCount: Int,
    val cancelled: Boolean,
)

/** Called as an install extracts files; return false to cancel */
fun interface ImportProgressListener {
    fun onProgress(done: Long, total: Long): Boolean
}
