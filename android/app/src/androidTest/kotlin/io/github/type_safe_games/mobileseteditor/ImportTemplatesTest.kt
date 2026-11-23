// Mobile Set Editor - import a Template zip through the JNI layer (#47)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.os.ParcelFileDescriptor
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.io.File
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.Outcome
import io.github.type_safe_games.mobileseteditor.core.Stores
import io.github.type_safe_games.mobileseteditor.core.TemplateImport
import io.github.type_safe_games.mobileseteditor.core.Verdict
import org.xmlpull.v1.XmlPullParser

/** A small zip made from the Test fixture: a symbol font under a new name, and a loose font */
@RunWith(AndroidJUnit4::class)
class ImportTemplatesTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()

        const val PACKAGE = "import-test-symbols.mse-symbol-font"
    }

    private fun testZip(): File {
        val files = TestFixture.assetFiles("data/fixture-symbols.mse-symbol-font")
            .mapKeys { "Pack/data/$PACKAGE/${it.key}" }
            .toMutableMap()
        files["Pack/Fonts/ImportTest.ttf"] = TestFixture.testAssets.open("fonts/DejaVuSans-Bold.ttf").use { it.readBytes() }
        return TestFixture.zip("import-test.zip", files)
    }

    @Test
    fun importsAndThenSkips() = runBlocking {
        val zip = testZip()
        ParcelFileDescriptor.open(zip, ParcelFileDescriptor.MODE_READ_ONLY).use { fd ->
            val scan = TemplateImport.scan(fd)
            assertTrue("scan: ${scan.result}", scan.result.ok)
            assertEquals(1, scan.packages.size)
            assertEquals(PACKAGE, scan.packages[0].name)
            assertEquals(Verdict.Install, scan.packages[0].verdict)
            assertEquals(1, scan.fontCount)
            assertEquals(1, scan.newFontCount)
            assertTrue(scan.installSize > scan.fontsSize && scan.fontsSize > 0 && scan.newFontsSize == scan.fontsSize)

            val progress = mutableListOf<Pair<Long, Long>>()
            val result = TemplateImport.installAll(fd, scan) { done, total -> progress += done to total }
            assertTrue("install: ${result.result}", result.result.ok)
            assertEquals(Outcome.Installed, result.packages.single().outcome)
            assertEquals(1, result.fontCount)
            assertTrue("progress: $progress", progress.isNotEmpty() && progress.last().first == progress.last().second)
            assertEquals(scan.installSize, progress.last().second)
        }
        assertTrue(File(Stores.templates(TestFixture.app), "$PACKAGE/symbol-font").exists())
        assertTrue(File(Stores.fonts(TestFixture.app), "ImportTest.ttf").exists())
        assertTrue(Core.refreshTemplates().ok)
        val installed = Core.listTemplates()
        assertTrue(installed.result.ok)
        assertTrue(installed.templates.any { it.name == PACKAGE && it.type == "symbol-font" })

        // the same zip again: everything is skipped
        ParcelFileDescriptor.open(zip, ParcelFileDescriptor.MODE_READ_ONLY).use { fd ->
            val scan = TemplateImport.scan(fd)
            assertEquals(Verdict.SkipNotNewer, scan.packages.single().verdict)
            assertEquals(0, scan.newFontCount)
            assertEquals(0L, scan.installSize)
            assertFalse(scan.anythingNew)
            val result = TemplateImport.install(fd, scan.packages.map { it.name })
            assertTrue(result.result.ok)
            assertEquals(Outcome.SkippedNotNewer, result.packages.single().outcome)
            assertEquals(0, result.fontCount) // (the same font is copied only once)
        }
    }

    @Test
    fun backupExcludesTheStores() {
        val resources = TestFixture.app.resources
        for (id in listOf(R.xml.data_extraction_rules, R.xml.backup_rules)) {
            val excluded = mutableListOf<String>()
            resources.getXml(id).use { xml ->
                while (xml.next() != XmlPullParser.END_DOCUMENT) {
                    if (xml.eventType == XmlPullParser.START_TAG && xml.name == "exclude" && xml.getAttributeValue(null, "domain") == "file") {
                        excluded += xml.getAttributeValue(null, "path")
                    }
                }
            }
            assertTrue("excluded: $excluded", excluded.containsAll(listOf("templates/", "fonts/")))
        }
        // and the stores are where those paths point
        assertEquals(File(TestFixture.app.filesDir, "templates"), Stores.templates(TestFixture.app))
        assertEquals(File(TestFixture.app.filesDir, "fonts"), Stores.fonts(TestFixture.app))
    }
}
