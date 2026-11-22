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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
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
import org.mobileseteditor.render.CoreCardRenderer
import org.mobileseteditor.render.RenderKey
import org.mobileseteditor.render.RenderService

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
 * Opens a Set (read-only for now: the Working copy, autosave and Save as come with M4) and shows
 * its Cards one at a time. Renders go through a [RenderService] (#59): the current Card first,
 * its neighbours ahead of time, all cached within a memory budget.
 */
class SetViewModel(app: Application) : AndroidViewModel(app) {
    private val _state = MutableStateFlow(SetState())
    val state: StateFlow<SetState> = _state

    /** How many Cards on each side of the current one are rendered ahead */
    var lookAhead = 2

    /** The open Set's render queue and cache (null while none is open) */
    var renders: RenderService<Bitmap>? = null
        private set
    private var rendersScope: CoroutineScope? = null
    private var observeJob: Job? = null
    private var width = 0
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
            // (the worker off the core thread: each render is then its own dispatch onto it, so
            // calls made meanwhile, such as edits, run between renders)
            val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
            rendersScope = scope
            renders = RenderService(
                CoreCardRenderer(open.session) { addMessages(it) },
                CoreCardRenderer.defaultBudget(),
                scope,
            )
            _state.value = SetState(
                session = open.session,
                cardUids = cards.uids.toList(),
                cardNames = cards.names.toList(),
                messages = messageLines(open.result) + messageLines(cards.result),
            )
            show()
        }
    }

    /** The width in pixels Cards are rendered at (the screen's) */
    fun setWidth(pixels: Int) {
        if (pixels > 0 && pixels != width) {
            width = pixels
            show()
        }
    }

    fun previous() = go(_state.value.index - 1)
    fun next() = go(_state.value.index + 1)

    fun go(index: Int) {
        val s = _state.value
        if (index !in s.cardUids.indices || index == s.index) return
        _state.value = s.copy(index = index, card = null)
        show()
    }

    /** Want the current Card and its neighbours, and show the current one's render */
    private fun show() {
        val s = _state.value
        val service = renders ?: return
        if (s.cardUids.isEmpty() || width <= 0) return
        val index = s.index
        val key = RenderKey(s.cardUids[index], width)
        val ahead = (1..lookAhead).flatMap { listOf(index + it, index - it) }
            .filter { it in s.cardUids.indices }
            .map { RenderKey(s.cardUids[it], width) }
        service.want(listOf(key), ahead)
        observeJob?.cancel()
        observeJob = viewModelScope.launch {
            service.observe(key).collect { r ->
                val current = _state.value
                if (r != null && current.index == index) {
                    _state.value = current.copy(
                        card = RenderedCard(index, key.uid, current.cardNames.getOrElse(index) { "" }, r.image, r.regions),
                    )
                }
            }
        }
    }

    private fun addMessages(messages: List<CoreMessage>) {
        val lines = messages.filter { it.severity != Severity.Info }.map(CoreMessage::toString)
        if (lines.isNotEmpty()) _state.value = _state.value.copy(messages = _state.value.messages + lines)
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
        observeJob?.cancel()
        rendersScope?.cancel()
        rendersScope = null
        renders = null
        val session = _state.value.session
        if (session != 0L) withContext(NonCancellable) { Core.closeSession(session) }
    }

    override fun onCleared() {
        rendersScope?.cancel()
        // (viewModelScope is gone by now; the core thread closes it on its own)
        val session = _state.value.session
        if (session != 0L) CoroutineScope(Core.dispatcher).launch { Core.closeSession(session) }
    }
}
