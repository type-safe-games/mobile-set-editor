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

/** A Card's link: [uid] is the linked Card, [relation] what it is to this one (e.g. "Back Face") */
data class CardLink(val uid: String, val relation: String)

class CardListResult(
    val result: CoreResult,
    val uids: Array<String>,
    val names: Array<String>,
    linkUids: Array<Array<String>>,
    linkRelations: Array<Array<String>>,
) {
    /** Each Card's links, in the order of [uids] */
    val links: List<List<CardLink>> = uids.indices.map { i -> linkUids[i].indices.map { CardLink(linkUids[i][it], linkRelations[i][it]) } }
}

/** A card link type: the Card linked from is [selected], the other one [linked] (e.g. Front Face, Back Face) */
data class LinkType(val selected: String, val linked: String)

class LinkTypeListResult(val result: CoreResult, selected: Array<String>, linked: Array<String>) {
    val types: List<LinkType> = selected.indices.map { LinkType(selected[it], linked[it]) }
}

class CardSizeResult(val result: CoreResult, val width: Double, val height: Double)

/** A field's region in a rendered card, in bitmap pixels */
class FieldRegion(
    val field: String,
    val x: Double, val y: Double, val width: Double, val height: Double,
    val rotation: Double,
    val z: Int,
    val editable: Boolean,
)

/** An item of a symbol font's insert-symbol menu; mirrors the facade's SymbolMenuItem */
class SymbolMenuItem(val type: Type, val label: String, val code: String, val prompt: String, val id: Int, val items: List<SymbolMenuItem>) {
    enum class Type {
        /** insert [code] as a symbol */
        Code,
        /** ask for a code ([prompt]) and insert that as a symbol */
        Custom,
        Line,
        Submenu,
    }
}

/** A text field's symbol menu: [hasSymbols] if it can hold symbols; [alwaysSymbol] if all its text is symbols */
class SymbolMenuResult(
    val result: CoreResult,
    val hasSymbols: Boolean,
    val alwaysSymbol: Boolean,
    types: IntArray,
    labels: Array<String>,
    codes: Array<String>,
    prompts: Array<String>,
    ids: IntArray,
    parents: IntArray,
) {
    /** The menu's top items (the JNI hands it over flattened, each item with its parent's index) */
    val items: List<SymbolMenuItem> = run {
        fun children(parent: Int): List<SymbolMenuItem> = types.indices.filter { parents[it] == parent }.map {
            SymbolMenuItem(SymbolMenuItem.Type.entries[types[it]], labels[it], codes[it], prompts[it], ids[it], children(it))
        }
        children(-1)
    }
}

/** A symbol preview as ARGB pixels; width 0 if the item has none */
class SymbolPreviewResult(val result: CoreResult, val width: Int, val height: Int, val pixels: IntArray)

/** The field under a tap; empty if none */
class FieldAtResult(val result: CoreResult, val field: String)

class RenderResult(val result: CoreResult, val regions: Array<FieldRegion>)

// ----------------------------------------------------------------------------- Templates

/** An installed Template */
class TemplateInfo(
    val name: String,
    val type: String,
    val fullName: String,
    val version: String,
    val game: String,
    /** for pickers */
    val shortName: String = fullName,
    /** the package's icon image, "" if it has none */
    val iconPath: String = "",
)

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
/** One of a color field's preset colors; [color] as setValue takes it, e.g. "rgb(255,0,0)" */
class ColorChoice(val name: String, val color: String)

/**
 * A field. [choices]: a choice, multiple choice or slider field's choice names (a slider's are its
 * numbers), or a package choice field's matching packages ("" first if none is allowed). [colors]
 * and [allowCustom]: a color field's presets, and whether other colors are allowed.
 */
class FieldInfo(
    val name: String,
    val type: String,
    val editable: Boolean,
    val multiLine: Boolean,
    val choices: List<String>,
    val colors: List<ColorChoice> = emptyList(),
    val allowCustom: Boolean = false,
)

class FieldListResult(
    val result: CoreResult,
    names: Array<String>,
    types: Array<String>,
    editable: BooleanArray,
    multiLine: BooleanArray,
    choices: Array<Array<String>>,
    colorNames: Array<Array<String>>,
    colorValues: Array<Array<String>>,
    allowCustom: BooleanArray,
) {
    val fields: List<FieldInfo> = names.indices.map { i ->
        FieldInfo(
            names[i], types[i], editable[i], multiLine[i], choices[i].toList(),
            colorNames[i].indices.map { ColorChoice(colorNames[i][it], colorValues[i][it]) },
            allowCustom[i],
        )
    }
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
