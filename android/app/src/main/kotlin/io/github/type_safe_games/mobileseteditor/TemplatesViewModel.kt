// Mobile Set Editor - the Templates screen's state
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.net.Uri
import android.os.ParcelFileDescriptor
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.ImportChoice
import io.github.type_safe_games.mobileseteditor.core.ImportResult
import io.github.type_safe_games.mobileseteditor.core.ScanResult
import io.github.type_safe_games.mobileseteditor.core.Stores
import io.github.type_safe_games.mobileseteditor.core.TemplateImport
import io.github.type_safe_games.mobileseteditor.core.TemplateInfo

/** Where an import is */
sealed interface ImportState {
    data object Idle : ImportState
    data object Scanning : ImportState
    /** Scanned: the user picks what to install ([choice]), then confirms */
    class Scanned(val scan: ScanResult, val choice: ImportChoice) : ImportState
    class Installing(val done: Long, val total: Long) : ImportState
    class Finished(val result: ImportResult) : ImportState
    data object Cancelled : ImportState
    class Failed(val message: String) : ImportState
}

/** The installed Templates, and importing more from a zip the user picks (ADR 0005, minimal for M3) */
class TemplatesViewModel(app: Application) : AndroidViewModel(app) {
    val installed = MutableStateFlow<List<TemplateInfo>>(emptyList())
    private val _import = MutableStateFlow<ImportState>(ImportState.Idle)
    val import: StateFlow<ImportState> = _import

    private var zip: ParcelFileDescriptor? = null
    private var installJob: Job? = null

    init {
        viewModelScope.launch {
            val start = Stores.startCore(getApplication())
            if (!start.ok) _import.value = ImportState.Failed("The core could not start: ${start.error}")
            refreshList()
        }
    }

    private suspend fun refreshList() {
        installed.value = Core.listTemplates().templates.toList()
    }

    /** The user picked a zip: scan it */
    fun picked(uri: Uri) {
        closeZip()
        _import.value = ImportState.Scanning
        viewModelScope.launch {
            val fd = try {
                getApplication<Application>().contentResolver.openFileDescriptor(uri, "r")
            } catch (e: Exception) {
                null
            }
            if (fd == null) {
                _import.value = ImportState.Failed("The file could not be opened")
                return@launch
            }
            zip = fd
            val scan = TemplateImport.scan(fd)
            _import.value = if (scan.result.ok) ImportState.Scanned(scan, ImportChoice.everything(scan)) else ImportState.Failed(scan.result.error)
        }
    }

    /** The user changed what to install */
    fun choose(change: (ImportChoice) -> ImportChoice) {
        val scanned = _import.value as? ImportState.Scanned ?: return
        _import.value = ImportState.Scanned(scanned.scan, change(scanned.choice))
    }

    /** Install the chosen packages (and the ones they need) from the scanned zip */
    fun confirm() {
        val scanned = _import.value as? ImportState.Scanned ?: return
        val fd = zip ?: return
        val choice = scanned.choice
        _import.value = ImportState.Installing(0, choice.size)
        installJob = viewModelScope.launch {
            try {
                var shown = -1L // (per mille, so a pack's tens of thousands of files don't each redraw)
                val result = TemplateImport.install(fd, choice.packages.toList(), choice.fonts, choice.fonts) { done, total ->
                    val permille = if (total > 0) done * 1000 / total else 0
                    if (permille != shown) {
                        shown = permille
                        _import.value = ImportState.Installing(done, total)
                    }
                }
                _import.value = if (result.result.ok) ImportState.Finished(result) else ImportState.Failed(result.result.error)
            } catch (e: CancellationException) {
                _import.value = ImportState.Cancelled
                throw e
            } finally {
                closeZip()
                // packages installed before a cancel stay installed (each one whole)
                withContext(NonCancellable) {
                    Core.refreshTemplates()
                    refreshList()
                }
            }
        }
    }

    /** Cancel the install, or forget the scan */
    fun cancel() {
        installJob?.cancel()
        if (_import.value is ImportState.Scanned) {
            closeZip()
            _import.value = ImportState.Idle
        }
    }

    fun dismiss() {
        if (_import.value !is ImportState.Installing) _import.value = ImportState.Idle
    }

    private fun closeZip() {
        zip?.close()
        zip = null
    }

    override fun onCleared() = closeZip()
}
