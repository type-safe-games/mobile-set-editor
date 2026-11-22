// Mobile Set Editor - Working copies, autosave, coming back after process death, Save and Save as (#60)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.io.File
import java.util.zip.ZipFile
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.TargetKind
import org.mobileseteditor.core.WorkingCopies

@RunWith(AndroidJUnit4::class)
class WorkingCopyTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    private val app get() = TestFixture.app.applicationContext as Application

    private suspend fun SetViewModel.waitFor(condition: (SetState) -> Boolean) = withTimeout(30_000) {
        while (!condition(state.value)) delay(10)
    }

    private suspend fun title(model: SetViewModel) =
        Core.getValue(model.state.value.session, TargetKind.Set, "", "title").value

    /** The "set" file in an .mse-set zip */
    private fun setText(file: File) = ZipFile(file).use { zip ->
        zip.getInputStream(zip.getEntry("set")).readBytes().toString(Charsets.UTF_8)
    }

    @Test
    fun anEditSurvivesProcessDeathAndSaves() = runBlocking {
        val document = TestFixture.zip("working-copy.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))
        val uri = Uri.fromFile(document)
        WorkingCopies.forUri(app, uri.toString())?.let { WorkingCopies.delete(app, it) }

        // open: a new Working copy; edit; the autosave follows shortly
        val first = SetViewModel(app).apply { autosaveDelayMillis = 100 }
        first.open(uri)
        first.waitFor { it.session != 0L }
        val copy = requireNotNull(first.state.value.workingCopy)
        assertFalse(first.state.value.dirty)
        first.testEdit().join()
        first.waitFor { it.dirty }
        assertEquals("Fixture cards (edited)", title(first))
        assertTrue(WorkingCopies.get(app, copy.id)!!.unsaved)
        withTimeout(10_000) { while (!File(copy.setDir, "set").readText().contains("(edited)")) delay(20) }

        // the process dies: the session goes without being closed by the app
        Core.closeSession(first.state.value.session)
        val again = SetViewModel(app)
        again.restore()
        again.waitFor { it.session != 0L }
        assertEquals(copy.id, again.state.value.workingCopy!!.id)
        assertTrue("restored as unsaved", again.state.value.dirty)
        assertEquals("Fixture cards (edited)", title(again))
        assertTrue(!setText(document).contains("(edited)")) // the document is untouched until Save

        // Save writes the document
        assertTrue(again.save())
        assertFalse(again.state.value.dirty)
        assertFalse(WorkingCopies.get(app, copy.id)!!.unsaved)
        assertTrue(setText(document).contains("title: Fixture cards (edited)"))

        // Save as writes a new document, which the Set belongs to from then on
        val other = File(TestFixture.work, "saved-as.mse-set").apply { delete() }
        assertTrue(again.saveAs(Uri.fromFile(other)))
        assertTrue(setText(other).contains("title: Fixture cards (edited)"))
        assertEquals(Uri.fromFile(other).toString(), WorkingCopies.get(app, copy.id)!!.uri)

        // closing keeps the Working copy; opening its document again reuses it
        again.close(CloseChoice.Keep).join()
        val third = SetViewModel(app)
        third.open(Uri.fromFile(other))
        third.waitFor { it.session != 0L }
        assertEquals(copy.id, third.state.value.workingCopy!!.id)
        third.close(CloseChoice.Keep).join()
    }

    @Test
    fun discardingUnsavedChangesRemovesTheWorkingCopy() = runBlocking {
        val document = TestFixture.zip("discard.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))
        val model = SetViewModel(app)
        model.open(Uri.fromFile(document))
        model.waitFor { it.session != 0L }
        val copy = model.state.value.workingCopy!!
        model.testEdit().join()
        model.waitFor { it.dirty }
        model.close(CloseChoice.Discard).join()
        assertTrue(WorkingCopies.get(app, copy.id) == null && !copy.root.exists())
        assertEquals(null, WorkingCopies.lastOpen(app))
    }
}
