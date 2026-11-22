// Mobile Set Editor - importing Templates from a zip (ADR 0005)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.core

import android.os.ParcelFileDescriptor
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext
import kotlin.coroutines.coroutineContext

/**
 * Template import, which runs on a background thread rather than the core thread (ADR 0009).
 *
 * Scan the zip first and show the user what it holds and how big it is, then install. The zip is
 * a file the user picked (the Storage Access Framework); the descriptor is read, never closed.
 * Cancelling the coroutine that installs cancels the install between files: each package is
 * installed whole or not at all. Afterwards call [Core.refreshTemplates].
 */
object TemplateImport {
    suspend fun scan(zip: ParcelFileDescriptor): ScanResult =
        withContext(Dispatchers.IO) { NativeCore.scanTemplatesFd(zip.fd) }

    /** Install these packages (and the ones in the zip they need), the loose fonts and dictionaries */
    suspend fun install(
        zip: ParcelFileDescriptor,
        packages: List<String>,
        onProgress: (done: Long, total: Long) -> Unit = { _, _ -> },
    ): ImportResult = withContext(Dispatchers.IO) {
        val context = coroutineContext
        NativeCore.installTemplatesFd(zip.fd, packages.toTypedArray()) { done, total ->
            onProgress(done, total)
            context.isActiveOrFalse()
        }.also { context.ensureActive() }
    }

    /** Install everything the scan says can be installed */
    suspend fun installAll(
        zip: ParcelFileDescriptor,
        scan: ScanResult,
        onProgress: (done: Long, total: Long) -> Unit = { _, _ -> },
    ): ImportResult = install(zip, scan.packages.filter { it.installable }.map { it.name }, onProgress)

    private fun kotlin.coroutines.CoroutineContext.isActiveOrFalse(): Boolean =
        this[kotlinx.coroutines.Job]?.isActive ?: true
}
