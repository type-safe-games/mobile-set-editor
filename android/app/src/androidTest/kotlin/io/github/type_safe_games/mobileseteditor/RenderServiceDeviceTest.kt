// Mobile Set Editor - stepping through a 300-Card Set with the render queue and cache (#59)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.delay
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.render.RenderKey

/**
 * A Set of 300 Cards made from the Test fixture's, viewed Card by Card as #48's viewer does: after
 * the first, each next Card has been rendered ahead, and the cache stays within its budget.
 */
@RunWith(AndroidJUnit4::class)
class RenderServiceDeviceTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()

        const val CARDS = 300
        const val WIDTH = 540
    }

    /** The fixture Set's three Cards, repeated with new uids (and no links) until there are [count] */
    private fun bigSet(count: Int): Map<String, ByteArray> {
        val files = TestFixture.assetFiles("sets/fixture-cards.mse-set").toMutableMap()
        val text = String(files.getValue("set"), Charsets.UTF_8)
        val head = text.substringBefore("card:\n")
        val cards = text.substring(head.length).split(Regex("(?m)^card:\n")).filter { it.isNotBlank() }
            .map { block -> block.lines().filter { !it.trimStart().startsWith("linked_") }.joinToString("\n") }
        val out = StringBuilder(head)
        for (i in 0 until count) {
            val block = cards[i % cards.size].replace(Regex("uid: \\S+"), "uid: big-$i")
            out.append("card:\n").append(block.trimEnd('\n')).append('\n')
        }
        files["set"] = out.toString().toByteArray(Charsets.UTF_8)
        return files
    }

    private suspend fun SetViewModel.waitFor(condition: (SetState) -> Boolean) = withTimeout(60_000) {
        while (!condition(state.value)) delay(10)
    }

    @Test
    fun stepsThroughWithCardsRenderedAheadWithinTheBudget() = runBlocking {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        model.open(Uri.fromFile(TestFixture.zip("big.mse-set", bigSet(CARDS))))
        model.waitFor { it.session != 0L || it.messages.isNotEmpty() }
        assertEquals("messages: ${model.state.value.messages}", CARDS, model.state.value.cardUids.size)
        model.setWidth(WIDTH)
        model.waitFor { it.card?.index == 0 }
        val renders = requireNotNull(model.renders)
        var maxBytes = 0L
        for (i in 1 until CARDS) {
            // the next Card is rendered ahead while this one is looked at (the test waits for that,
            // as a user would take a moment), without being asked for
            val next = RenderKey(model.state.value.cardUids[i], WIDTH)
            withTimeout(60_000) { while (next !in renders.cachedKeys) delay(5) }
            model.next()
            // shown at once: it is in the cache
            val shownAt = System.nanoTime()
            model.waitFor { it.card?.index == i }
            assertTrue("card $i took ${(System.nanoTime() - shownAt) / 1_000_000} ms", System.nanoTime() - shownAt < 200_000_000)
            maxBytes = maxOf(maxBytes, renders.cachedBytes)
            assertTrue("card $i: ${renders.cachedBytes} bytes over the budget of ${renders.budgetBytes}", renders.cachedBytes <= renders.budgetBytes)
        }
        // the budget was used: older renders were evicted
        assertTrue("the cache never filled ($maxBytes of ${renders.budgetBytes})", renders.cachedKeys.size < CARDS)
    }
}
