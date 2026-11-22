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

/** The field under a tap; empty if none */
class FieldAtResult(val result: CoreResult, val field: String)

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

// ----------------------------------------------------------------------------- Sessions and values (ADR 0009)

/** What an action made stale: [all] Cards, or the Cards [cardUids]; [cardList] if Cards were added, removed or moved */
class Stale(val all: Boolean, val cardUids: List<String>, val cardList: Boolean)

/** The result of anything that changes the Set */
class ActionResult(
    val result: CoreResult,
    staleAll: Boolean,
    staleUids: Array<String>,
    staleCardList: Boolean,
    /** does the Set differ from what was last saved? */
    val dirty: Boolean,
    val canUndo: Boolean,
    val canRedo: Boolean,
    val undoName: String,
    val redoName: String,
) {
    val stale = Stale(staleAll, staleUids.toList(), staleCardList)
}

class SessionInfo(
    val result: CoreResult,
    val game: String,
    val stylesheet: String,
    val dirty: Boolean,
    val canUndo: Boolean,
    val canRedo: Boolean,
)

class ValueResult(val result: CoreResult, val type: String, val value: String)

/** What a value belongs to; mirrors the facade's Target::Kind, in its order */
enum class TargetKind { Card, Set, CardStyle, SetStyle, Keyword }

// ----------------------------------------------------------------------------- Fields and text editing (ADR 0009)

/** A field of a Card, the Set info or a Style's options */
class FieldInfo(val name: String, val type: String, val editable: Boolean, val multiLine: Boolean, val choices: List<String>)

class FieldListResult(
    val result: CoreResult,
    names: Array<String>,
    types: Array<String>,
    editable: BooleanArray,
    multiLine: BooleanArray,
    choices: Array<Array<String>>,
) {
    val fields: List<FieldInfo> = names.indices.map { FieldInfo(names[it], types[it], editable[it], multiLine[it], choices[it].toList()) }
}

/**
 * A text field's state for an editor: its visible text, the selection after the call and the locked
 * spans (each replaced or removed as a whole). Positions count Unicode code points, not UTF-16 units.
 */
class TextResult(
    val action: ActionResult,
    val text: String,
    val selectionStart: Int,
    val selectionEnd: Int,
    lockedStarts: IntArray,
    lockedEnds: IntArray,
) {
    val locked: List<IntRange> = lockedStarts.indices.map { lockedStarts[it] until lockedEnds[it] }
}
