// Mobile Set Editor - an open Set, its Cards rendered one at a time
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.app.Application
import android.graphics.Bitmap
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import java.io.File
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.CoreMessage
import org.mobileseteditor.core.CoreResult
import org.mobileseteditor.core.FieldRegion
import org.mobileseteditor.core.Severity
import org.mobileseteditor.core.Status
import org.mobileseteditor.core.Stores

/** A Card as rendered for the screen */
class RenderedCard(val index: Int, val uid: String, val name: String, val bitmap: Bitmap, val regions: List<FieldRegion>)

/** What the Set screen shows */
data class SetState(
    val opening: Boolean = false,
    val session: Long = 0,
    val cardUids: List<String> = emptyList(),
    val cardNames: List<String> = emptyList(),
    val index: Int = 0,
    /** null while the current Card renders */
    val card: RenderedCard? = null,
    /** what the core said, newest last (errors, warnings) */
    val messages: List<String> = emptyList(),
)

/**
 * Opens a Set (read-only for now: the Working copy, autosave and Save as come with M4) and renders
 * its Cards one at a time, on the core thread, at the width the screen asks for (ADR 0009).
 */
class SetViewModel(app: Application) : AndroidViewModel(app) {
    private val _state = MutableStateFlow(SetState())
    val state: StateFlow<SetState> = _state

    private var width = 0
    private var renderJob: Job? = null
    private var sessionCounter = 0

    /** Open the Set the user picked; any open one is closed */
    fun open(uri: Uri) {
        viewModelScope.launch {
            closeSession()
            _state.value = SetState(opening = true)
            val start = Stores.startCore(getApplication())
            if (!start.ok) return@launch report(start, "The core could not start")
            val fd = try {
                getApplication<Application>().contentResolver.openFileDescriptor(uri, "r")
            } catch (e: Exception) {
                null
            }
            if (fd == null) {
                _state.value = SetState(messages = listOf("The file could not be opened"))
                return@launch
            }
            val work = File(getApplication<Application>().cacheDir, "sessions/${System.currentTimeMillis()}-${++sessionCounter}")
            val open = fd.use { Core.openSession(it, work) }
            if (!open.result.ok) return@launch report(open.result, "The Set could not be opened")
            val cards = Core.listCards(open.session)
            _state.value = SetState(
                session = open.session,
                cardUids = cards.uids.toList(),
                cardNames = cards.names.toList(),
                messages = messageLines(open.result) + messageLines(cards.result),
            )
            render()
        }
    }

    /** The width in pixels Cards are rendered at (the screen's) */
    fun setWidth(pixels: Int) {
        if (pixels > 0 && pixels != width) {
            width = pixels
            render()
        }
    }

    fun previous() = go(_state.value.index - 1)
    fun next() = go(_state.value.index + 1)

    private fun go(index: Int) {
        val s = _state.value
        if (index !in s.cardUids.indices || index == s.index) return
        _state.value = s.copy(index = index, card = null)
        render()
    }

    private fun render() {
        val s = _state.value
        if (s.session == 0L || s.cardUids.isEmpty() || width <= 0) return
        renderJob?.cancel()
        val index = s.index
        val uid = s.cardUids[index]
        val w = width
        renderJob = viewModelScope.launch {
            val size = Core.cardSize(s.session, uid)
            if (!size.result.ok || size.width <= 0) return@launch report(size.result, "The Card has no size")
            val h = (w * size.height / size.width).toInt().coerceAtLeast(1)
            val bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
            val r = Core.renderCard(s.session, uid, bitmap)
            val current = _state.value
            if (current.session != s.session || current.index != index) return@launch
            _state.value = current.copy(
                card = RenderedCard(index, uid, current.cardNames.getOrElse(index) { "" }, bitmap, r.regions.toList()),
                messages = current.messages + messageLines(r.result),
            )
        }
    }

    private fun report(result: CoreResult, what: String) {
        val line = when (result.status) {
            Status.MissingTemplate -> "$what: the Template ${result.missingPackage} is not installed"
            else -> "$what: ${result.error}"
        }
        _state.value = _state.value.copy(opening = false, messages = _state.value.messages + messageLines(result) + line)
    }

    private fun messageLines(result: CoreResult): List<String> =
        result.messages.filter { it.severity != Severity.Info }.map(CoreMessage::toString)

    private suspend fun closeSession() {
        renderJob?.cancel()
        val session = _state.value.session
        if (session != 0L) withContext(NonCancellable) { Core.closeSession(session) }
    }

    override fun onCleared() {
        // (viewModelScope is gone by now; the core thread closes it on its own)
        val session = _state.value.session
        if (session != 0L) CoroutineScope(Core.dispatcher).launch { Core.closeSession(session) }
    }
}
