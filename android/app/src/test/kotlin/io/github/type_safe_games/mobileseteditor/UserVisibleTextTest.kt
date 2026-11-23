// Mobile Set Editor - no user-visible text names Magic, its publisher or a Template pack (#76)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import java.io.File
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * ADR 0008: the app never says "Magic"; ADR 0005: it never names, links to or downloads a Template
 * pack. The app's text is in Kotlin string literals and the XML resources, so this reads both from
 * the sources (comments don't count: nobody sees them). Package names in the Style support table
 * (assets/style-status.tsv, #72) are data matched against the user's own Templates, not text.
 */
class UserVisibleTextTest {
    private val banned = Regex("""magic|\bmtg\b|wizards of the coast|\bwotc\b|gathering|planeswalker""", RegexOption.IGNORE_CASE)

    /** Unit tests run in the module directory (android/app) */
    private val main = File("src/main")

    /** The string literals of Kotlin source, without comments */
    private fun literals(source: String): List<String> {
        val out = mutableListOf<String>()
        var i = 0
        while (i < source.length) {
            when {
                source.startsWith("//", i) -> i = source.indexOf('\n', i).let { if (it < 0) source.length else it }
                source.startsWith("/*", i) -> i = source.indexOf("*/", i + 2).let { if (it < 0) source.length else it + 2 }
                source.startsWith("\"\"\"", i) -> {
                    val end = source.indexOf("\"\"\"", i + 3).let { if (it < 0) source.length else it }
                    out += source.substring(i + 3, end); i = end + 3
                }
                source[i] == '"' -> {
                    var j = i + 1
                    while (j < source.length && source[j] != '"' && source[j] != '\n') j += if (source[j] == '\\') 2 else 1
                    out += source.substring(i + 1, minOf(j, source.length)); i = j + 1
                }
                source[i] == '\'' && i + 2 < source.length -> i += if (source[i + 1] == '\\') 4 else 3
                else -> i++
            }
        }
        return out
    }

    @Test
    fun findsLiteralsButNotComments() {
        assertEquals(listOf("a", "b \\\" c", "multi\nline"), literals("// \"x\"\n/* \"y\" */ f(\"a\", '\"', \"b \\\" c\") \"\"\"multi\nline\"\"\""))
    }

    @Test
    fun theAppSaysNothingAboutMagic() {
        val sources = main.resolve("kotlin").walk().filter { it.extension == "kt" }.toList()
        assertTrue("no Kotlin sources under ${main.absolutePath}", sources.isNotEmpty())
        val hits = sources.flatMap { file ->
            literals(file.readText()).filter { banned.containsMatchIn(it) }.map { "${file.name}: \"$it\"" }
        } + main.resolve("res").walk().filter { it.extension == "xml" }.flatMap { file ->
            file.readLines().filter { banned.containsMatchIn(it.substringBefore("<!--")) }.map { "${file.name}: $it" }
        } + listOf(main.resolve("AndroidManifest.xml")).flatMap { file ->
            file.readLines().filter { banned.containsMatchIn(it.substringBefore("<!--")) }.map { "${file.name}: $it" }
        }
        assertTrue("user-visible text names Magic, its publisher or a Template pack:\n" + hits.joinToString("\n"), hits.isEmpty())
    }
}
