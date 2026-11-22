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
